#include <pep/async/CreateObservable.hpp>
#include <pep/async/ObservableAwaiter.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/auth/OAuthError.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/client/Client.hpp>
#include <pep/oauth-client/OAuthClient.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/ThreadUtil.hpp>
#include <pep/weblib/EmscriptenValPtr.hpp>
#include <pep/weblib/EmscriptenVectorBinding.hpp>
#include <pep/weblib/ObservableByteStream.hpp>
#include <pep/weblib/ObservableStream.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>
#include <pep/weblib/ThreadPrintable.hpp>
#include <pep/weblib/WeblibApiPromise.hpp>
#include <pep/weblib/WeblibStructs.hpp>

#include <numeric>
#include <thread>

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/noncopyable.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-distinct_until_changed.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/subjects/rx-replaysubject.hpp>

using namespace pep;
using namespace pep::weblib;
using namespace emscripten;
using namespace std::literals;
using namespace std::ranges;

namespace {
const std::string LOG_TAG = "Weblib";

class Weblib final : public std::enable_shared_from_this<Weblib>, public SharedConstructor<Weblib>, public boost::noncopyable {
  friend class SharedConstructor;

  Configuration clientConfig_;
  std::shared_ptr<Client> client_;

  std::optional<rxcpp::observe_on_one_worker> asioWorker_, emscriptenMainWorker_;

  // Destruct these first to stop io_service
  std::jthread thread_;
  // Destruct before thread
  std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard_;

  Weblib() {
    // Set up event loop
    auto io_context = std::make_shared<boost::asio::io_context>();
    workGuard_.emplace(make_work_guard(*io_context));
    asioWorker_.emplace(observe_on_asio(*io_context));
    emscriptenMainWorker_.emplace(observe_on_emscripten_main_thread());

    clientConfig_ = Configuration::FromFile("ClientConfig.json");

    // Start client
    client_ = Client::OpenClient(clientConfig_, std::move(io_context));
    // Run event loop in background thread. Calls from JS will still come from the main thread.
    thread_ = std::jthread([io_context = client_->getIoContext()](std::stop_token stop) {
      ThreadName::Set("Client");
      LOG(LOG_TAG, debug) << "starting io_context " << io_context << " on thread " << ThreadPrintable{};
      std::stop_callback onStop(std::move(stop), [io_context] {
        LOG(LOG_TAG, debug) << "stopping io_context...";
        io_context->stop();
      });
      io_context->run();
      LOG(LOG_TAG, debug) << "io_context stopped";
    });
  }

  rxcpp::observable<bool> connectionStatusChange() {
    auto disconnectedServers = std::make_shared<std::unordered_set<std::string>>();
    std::map<std::string, std::shared_ptr<const ServerProxy>> servers{
      {"KeyServer", client_->getKeyServerProxy()},
      {"StorageFacility", client_->getStorageFacilityProxy()},
      {"AccessManager", client_->getAccessManagerProxy()},
      {"Transcryptor", client_->getTranscryptorProxy()},
      {"RegistrationServer", client_->getRegistrationServerProxy()},
      {"AuthServer", client_->getAuthServerProxy()},
    };

    auto serverConnectionStatuses = std::move(servers)
        | views::transform([](const auto& entry) {
          auto& [name, proxy] = entry;
          return proxy->connectionStatus()
              .map([name](ConnectionStatus status) {
                return std::pair{name, status};
              });
        });

    auto mergedConnectionStatuses = std::reduce(serverConnectionStatuses.begin(), serverConnectionStatuses.end(),
      rxcpp::observable<>::empty<std::pair<std::string, ConnectionStatus>>().as_dynamic(),
      [](const auto& obs1, const auto& obs2) { return obs1.merge(obs2); });

    return mergedConnectionStatuses
        .map([disconnectedServers = std::move(disconnectedServers)]
        (const std::pair<std::string, ConnectionStatus>& pair) {
              auto& [server, status] = pair;
              if (status.connected) {
                LOG(LOG_TAG, debug) << "Reconnected to " << server;
                disconnectedServers->erase(server);
              } else {
                if (disconnectedServers->insert(server).second) {
                  LOG(LOG_TAG, debug) << "Lost connection to " << server;
                }
              }
              return disconnectedServers->empty();
            }).distinct_until_changed();
  }

  /// Call this to make sure state is only accessed from the io_context thread
  rxcpp::observable<std::shared_ptr<Weblib>> onIoThread() {
    return rxcpp::observable<>::just(shared_from_this())
        .subscribe_on(*asioWorker_);
  }

public:
  /// Register \p callback to be called when the connection status changes.
  /// \param callback <code>(connected: bool) => void</code>
  void onStatusChange(val callback) {
    connectionStatusChange() // Safe to call outside io_context thread
        .observe_on(*emscriptenMainWorker_)
        // Even move constructor only allowed on main thread, avoid calling it when copying lambda
        .subscribe([callback = EmscriptenValPtr(std::move(callback))](bool connected) {
          (*callback)(connected);
        });
  }

  /// \returns \c Promise<EnrolledUser>
  WeblibApiPromise getEnrolledUser() {
    co_return co_await onIoThread()
      .map([](const std::shared_ptr<Weblib>& self) -> std::optional<EnrolledUser> {
        if (self->client_->getEnrolled()) {
          return EnrolledUser{
              .userGroup = self->client_->getEnrolledGroup(),
              .user = self->client_->getEnrolledUser(),
          };
        }
        return {};
      });
  }

  WeblibApiVoidPromise unenroll() {
    co_await onIoThread()
      .map([](const std::shared_ptr<Weblib>& self) {
        self->client_->unenroll();
        return FakeVoid{};
      });
  }

  /// Helper object for OAuth authentication flow
  class OAuthClient {
    rxcpp::subjects::replay<AuthorizationResult, decltype(asioWorker_)::value_type> authorizationCodeChan_;
    rxcpp::observable<FakeVoid> run_;

  public:
    OAuthClient(const std::shared_ptr<Weblib>& weblib, std::string redirectUri, val openAuthPage)
      : authorizationCodeChan_(*weblib->asioWorker_) {
      auto oauthClient = pep::OAuthClient::Create(pep::OAuthClient::Parameters{
        .io_context = weblib->client_->getIoContext(),
        .config = weblib->clientConfig_.get_child("AuthenticationServer"),
        .authorizationMethod = [this,
          redirectUri = std::move(redirectUri),
          openAuthPage = EmscriptenValPtr(std::move(openAuthPage))](
        const std::shared_ptr<boost::asio::io_context>&,
        const std::function<std::string(std::string redirectUri)>& getAuthorizeUri
        ) {
          // Still called on same thread
          (*openAuthPage)(getAuthorizeUri(redirectUri));
          // Would be nice to let openAuthPage return a promise,
          // but then we have to add full observable coroutine support,
          // plus val::awaiter currently only works inside val::promise_type, so callback it is
          return authorizationCodeChan_.get_observable();
        },
      });

      run_ = oauthClient->run()
          .flat_map([weblib](AuthorizationResult res) {
            if (res) {
              return weblib->client_->enrollUser(*res);
            } else {
              std::rethrow_exception(res.exception());
            }
          })
          .map([](const EnrollmentResult&) {
            LOG(LOG_TAG, pep::info) << "Completed enrollment!";
            return FakeVoid{};
          });
    }

    /// Call on successful authentication
    WeblibApiVoidPromise completeAuthentication(std::string code) {
      LOG(LOG_TAG, debug) << "completeAuthentication";
      rxcpp::observable<>::just(AuthorizationResult::Success(std::move(code)))
          .subscribe(authorizationCodeChan_.get_subscriber());
      co_await run_;
    }

    /// Call on authentication failure
    WeblibApiVoidPromise failAuthentication(std::string error, std::string errorDescription) {
      LOG(LOG_TAG, debug) << "failAuthentication";
      rxcpp::observable<>::just(AuthorizationResult::Failure(std::make_exception_ptr(
              OAuthError(std::move(error), std::move(errorDescription)))))
          .subscribe(authorizationCodeChan_.get_subscriber());
      co_await run_;
    }
  };

  /// Start OAuth authentication flow
  OAuthClient authenticate(std::string redirectUri, val openAuthPage) {
    LOG(LOG_TAG, debug) << "authenticate";
    return OAuthClient(shared_from_this(), std::move(redirectUri), std::move(openAuthPage));
  }

  WeblibApiVoidPromise authenticateWithToken(std::string token) {
    co_await onIoThread()
      .flat_map([token = std::move(token)](const std::shared_ptr<Weblib>& self) {
        return self->client_->enrollUser(token).op(RxInstead(FakeVoid{}));
      });
  }

  /// For development use: generate OAuth token from secret
  /// \returns \c Promise<string>
  auto internalGenerateToken(std::string tokenSecretHex, std::string userGroup) {
    using namespace std::chrono;
    auto now = TimeNow<sys_seconds>();
    return OAuthToken::Generate(
        boost::algorithm::unhex(tokenSecretHex),
        "weblib",
        userGroup,
        now,
        now + days{1})
      .getSerializedForm();
  }

  /// \returns \c Promise<ColumnGroup[]>
  WeblibApiPromise listColumns() {
    co_return co_await onIoThread()
        .flat_map([](const std::shared_ptr<Weblib>& self) {
          return self->client_->getAccessManagerProxy()->getAccessibleColumns(true, { "read" });
        })
        .map([](const ColumnAccess& access) {
          return RangeToVector(
            access.columnGroups
            | views::transform([&](const auto& entry) {
              const auto& [name, columnGroup] = entry;
              return ColumnGroup{
                  .name = name,
                  .columns = RangeToVector(SafeIndexInto(columnGroup.columns.mIndices, access.columns)),
              };
            })
          );
        });
  }

  /// \returns \c Promise<SubjectGroup[]>
  WeblibApiPromise listSubjectGroups() {
    co_return co_await onIoThread()
        .flat_map([](const std::shared_ptr<Weblib>& self) {
          return self->client_->getAccessManagerProxy()->getAccessibleParticipantGroups(true);
        }).map([](ParticipantGroupAccess access) {
          return RangeToVector(
            std::move(access).participantGroups
            | views::filter([](const auto& entry) {
              const auto& [name, modes] = entry;
              return find(modes, "access") != modes.end();
            })
            | views::keys
            | views::transform([](const std::string& name) {
              return SubjectGroup{ .name = name };
            })
          );
        });
  }

  /// \returns \c ReadableStream<CellEntry>
  auto list(ListQuery query) {
    return CreateReadableStreamOnMain(
        onIoThread()
        .flat_map([query = MakeSharedCopy(std::move(query))](const std::shared_ptr<Weblib>& self) {
          auto pps = query->subjects
                     ? self->client_->parsePpsOrIdentities(*query->subjects)
                       .op(RxGetOne("PolymorphicPseudonym vector"))
                       .as_dynamic()
                     : rxcpp::observable<>::just(std::make_shared<std::vector<PolymorphicPseudonym>>())
                       .as_dynamic();

          return pps.flat_map([self, query](const std::shared_ptr<std::vector<PolymorphicPseudonym>>& pps) {
            return self->client_->requestTicket2(requestTicket2Opts{
                .participantGroups = std::move(query->subjectGroups).value_or(std::vector<std::string>{}),
                .pps = std::move(*pps),
                .columnGroups = std::move(query->columnGroups).value_or(std::vector<std::string>{}),
                .columns = std::move(query->columns).value_or(std::vector<std::string>{}),
                .modes = {"read"}, //TODO support more modes. Should this method be split with an explicit ticket request?
                .includeAccessGroupPseudonyms = true,
              });
          });
        })
        .flat_map([client = client_](const IndexedTicket2& indexedTicket) {
          return client->enumerateData(indexedTicket.getTicket())
            .flat_map([](std::vector<std::shared_ptr<EnumerateResult>> chunk) {
              return rxcpp::observable<>::iterate(std::move(chunk));
            })
            .map([signedTicket = indexedTicket.getTicket()](std::shared_ptr<EnumerateResult> result) {
              //TODO rx-notification equals should return typename std::enable_if<std::is_same<decltype(lhs == rhs), bool>::value, bool>::value
              return CellEntry{std::move(result), signedTicket};
            });
        })
      );
  }

  /// \returns \c ReadableStream<CellData>
  auto retrieve(std::vector<const CellEntry*> entries) {
    return CreateReadableStreamOnMain(
        onIoThread()
        .flat_map([entries = MakeSharedCopy(std::move(entries))](
            const std::shared_ptr<Weblib>& self) -> rxcpp::observable<CellData> {
              if (entries->empty()) { return rxcpp::observable<>::empty<CellData>(); }

              auto signedTicket = entries->front()->ticket;
              if (!all_of(*entries, [&signedTicket](const CellEntry* entry) {
                return entry->ticket == signedTicket;
              })) {
                throw std::invalid_argument("Entry tickets differ. Can only retrieve entries from the same list call.");
              }

              //TODO Add ability to retrieve metadata. Should this method be split with an explicit getKeys?
              auto pageBatches = self->client_->retrieveData(
                  self->client_->getKeys(
                      rxcpp::observable<>::iterate(
                          RangeToVector(*entries | views::transform(std::mem_fn(&CellEntry::inner)))),
                      signedTicket),
                  signedTicket);
              return rxcpp::observable<>::just(entries)
                  .observe_on(*self->emscriptenMainWorker_)
                  .flat_map([self, pageBatches](const std::shared_ptr<std::vector<const CellEntry*>>& entries) {

                    auto cellStreams = std::make_shared<std::vector<rxcpp::subjects::subject<std::string>>>(entries->size());

                    std::vector<CellData> cellDatas;
                    cellDatas.reserve(entries->size());
                    for (std::size_t cellNum{}; cellNum < entries->size(); ++cellNum) {
                      cellDatas.emplace_back((*entries)[cellNum],
                          CreateReadableByteStream((*cellStreams)[cellNum].get_observable(), 1 << 20 /*1 MiB*/));
                    }

                    pageBatches
                        //TODO Do not request all batches at once, by implementing `pull` with ReadableStream!
                        .concat()
                        .observe_on(*self->emscriptenMainWorker_)
                        .subscribe(
                            // on next
                            [cellStreams](RetrievePage page) noexcept {
                              auto stream = (*cellStreams)[page.fileIndex].get_subscriber();
                              if (!page.content.empty()) {
                                stream.on_next(std::move(page.content));
                              }
                              if (page.last) { stream.on_completed(); }
                            },
                            // on error
                            [cellStreams](const std::exception_ptr& ex) noexcept {
                              LOG(LOG_TAG, debug) << "Error retrieving file pages: " << GetExceptionMessage(ex);
                              for (auto& stream: *cellStreams) {
                                // Only when on_completed has not been called
                                if (stream.get_subscription().is_subscribed()) {
                                  stream.get_subscriber().on_error(ex);
                                }
                              }
                              LOG(LOG_TAG, debug) << "Closed non-completed streams";
                            },
                            // on completed
                            [cellStreams]() noexcept {
                              for ([[maybe_unused]] auto& stream: *cellStreams) {
                                assert(!stream.get_subscription().is_subscribed()
                                    && "Not all files were completed?");
                              }
                              LOG(LOG_TAG, debug) << "Completed retrieve";
                            });
                    return rxcpp::observable<>::iterate(std::move(cellDatas));
                  });
            })
        );
  }

  /// \returns \c Promise<string>
  WeblibApiPromise registerParticipant(ParticipantPersonalia personalia, bool isTestParticipant) {
    co_return co_await onIoThread()
        .flat_map([personalia = std::move(personalia), isTestParticipant](const std::shared_ptr<Weblib>& self) {
          return self->client_->registerParticipant(personalia, isTestParticipant);
        });
  }
};

void exitRuntime() { ::emscripten_force_exit(0); }

EMSCRIPTEN_BINDINGS(weblib) {
  class_<Weblib>("Weblib")
      .smart_ptr_constructor("Weblib", &Weblib::Create<>)
      .function("onStatusChange", &Weblib::onStatusChange)
      .function("getEnrolledUser", &Weblib::getEnrolledUser)
      .function("unenroll", &Weblib::unenroll)
      .function("authenticate", &Weblib::authenticate)
      .function("authenticateWithToken", &Weblib::authenticateWithToken)
      .function("internalGenerateToken", &Weblib::internalGenerateToken)
      .function("listColumns", &Weblib::listColumns)
      .function("listSubjectGroups", &Weblib::listSubjectGroups)
      .function("registerParticipant", &Weblib::registerParticipant)
      .function("list", &Weblib::list)
      .function("retrieve", &Weblib::retrieve)
  ;
  class_<Weblib::OAuthClient>("OAuthClient")
      .function("completeAuthentication", &Weblib::OAuthClient::completeAuthentication)
      .function("failAuthentication", &Weblib::OAuthClient::failAuthentication)
  ;
  value_object<ParticipantPersonalia>("ParticipantPersonalia")
      .field("firstName", &ParticipantPersonalia::firstName)
      .field("middleName", &ParticipantPersonalia::middleName)
      .field("lastName", &ParticipantPersonalia::lastName)
      .field("dateOfBirth", &ParticipantPersonalia::dateOfBirth)
  ;
  register_optional<EnrolledUser>();

  function("exit", &exitRuntime);
}
}

int main() {
  // See limitations: https://emscripten.org/docs/porting/exceptions.html#limitations-regarding-std-terminate
  std::set_terminate([] {
    try {
      std::exception_ptr ex = std::current_exception();
      if (!ex) {
        std::cerr << "Originally thrown exception could not be retrieved in WASM EH, see https://github.com/emscripten-core/emscripten/issues/23779\n"
          << "Either add a 'caught exception' breakpoint in the debugger or switch to Emscripten EH" << std::endl;
      }
      std::cerr << "Error: " << GetExceptionMessage(std::move(ex)) << std::endl;
    } catch (...) {
      /*ignore: we did what we could*/
    }
    std::abort();
  });

  const bool isWeb = val::global("process").isUndefined();
  if (isWeb) {
    // Can likely be filtered via browser console (labeled 'debug', see JsConsoleLogging)
    Logging::Initialize({std::make_shared<JsConsoleLogging>(verbose)});
  } else {
    // Do not use console.log on Node as messages from worker threads will not immediately be printed
    // See https://github.com/nodejs/node/issues/30491
    Logging::Initialize({std::make_shared<ConsoleLogging>(verbose)});
  }

  // We exit `main`, but keep the runtime alive. That is, global destructors are not run,
  // and all other threads will keep running (in their WebWorkers),
  // and we can still call from JS into the main WebWorker.
  // See https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_exit_with_live_runtime
  // JS will call into our `exit` binding that will actually shut down the application.
  ::emscripten_exit_with_live_runtime();
}
