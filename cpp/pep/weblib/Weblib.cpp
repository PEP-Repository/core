#include <pep/async/CreateObservable.hpp>
#include <pep/async/EmscriptenValPtr.hpp>
#include <pep/async/ObservableAwaiter.hpp>
#include <pep/async/OnEmscriptenThread.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/auth/OAuthError.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/client/Client.hpp>
#include <pep/oauth-client/OAuthClient.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/weblib/EmscriptenVectorBinding.hpp>
#include <pep/weblib/ObservableByteStream.hpp>
#include <pep/weblib/ObservableStream.hpp>
#include <pep/weblib/WeblibApiPromise.hpp>
#include <pep/weblib/WeblibStructs.hpp>

#include <numeric>
#include <thread>

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <boost/algorithm/hex.hpp>
#include <boost/asio/io_context.hpp>
#include <emscripten/threading.h>
#include <pep/async/RxInstead.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-distinct_until_changed.hpp>
#include <rxcpp/operators/rx-finally.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep;
using namespace pep::weblib;
using namespace emscripten;
using namespace std::literals;
using namespace std::ranges;

namespace {
const std::string LOG_TAG = "Weblib";

class Weblib final : public std::enable_shared_from_this<Weblib>, public SharedConstructor<Weblib> {
  friend class SharedConstructor;

  Configuration clientConfig_;
  std::shared_ptr<Client> client_;
  std::shared_ptr<OAuthClient> oauthClient_;

  rxcpp::subjects::subject<AuthorizationResult> authorizationCodeChan_;

  // Destruct these first to stop io_service
  std::jthread thread_;
  // Destruct before thread
  std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard_;

  Weblib() {
    // Set up event loop
    auto io_context_ = std::make_shared<boost::asio::io_context>();
    workGuard_.emplace(make_work_guard(*io_context_));

    clientConfig_ = Configuration::FromFile("ClientConfig.json");

    // Start client
    client_ = Client::OpenClient(clientConfig_, std::move(io_context_));
    thread_ = std::jthread([io_context = client_->getIoContext()](std::stop_token stop) {
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
                LOG(LOG_TAG, debug) << "Lost connection to " << server;
                disconnectedServers->insert(server);
              }
              return disconnectedServers->empty();
            }).distinct_until_changed();
  }

  // Call this to make sure the Client is only accessed from the io_context thread
  rxcpp::observable<std::shared_ptr<Client>> onIoThread() const {
    return rxcpp::observable<>::just(client_)
        .subscribe_on(observe_on_asio(*client_->getIoContext()));
  }

public:
  Weblib(Weblib &&) = delete;

  /// Register \p callback to be called when the connection status changes.
  void onStatusChange(val callback) {
    connectionStatusChange() // Safe to call outside io_context thread
        .observe_on(observe_on_emscripten())
        // Even move constructor only allowed on main thread, avoid calling it when copying lambda
        .subscribe([callback = EmscriptenValPtr(std::move(callback))](bool connected) {
          (*callback)(connected);
        });
  }

  /// \returns Promise<EnrolledUser>
  WeblibApiPromise getEnrolledUser() {
    co_return co_await onIoThread()
      .map([](const std::shared_ptr<Client>& client) -> std::optional<EnrolledUser> {
        if (client->getEnrolled()) {
          return EnrolledUser{
              .userGroup = client->getEnrolledGroup(),
              .user = client->getEnrolledUser(),
          };
        }
        return {};
      });
  }

  //TODO The threading here is questionable
  WeblibApiVoidPromise authenticate(std::string redirectUri, val openAuthPage) {
    LOG(LOG_TAG, debug) << "authenticate";
    if (oauthClient_) {
      throw std::logic_error("Authentication already in progress");
    }

    oauthClient_ = OAuthClient::Create(OAuthClient::Parameters{
        .io_context = client_->getIoContext(),
        .config = clientConfig_.get_child("AuthenticationServer"),
        .authorizationMethod = [
            redirectUri,
            openAuthPage = EmscriptenValPtr(openAuthPage),
            self = shared_from_this()
        ](
            std::shared_ptr<boost::asio::io_context> io_context,
            std::function<std::string(std::string redirectUri)> getAuthorizeUri
        ) {
          // Still called on same thread
          (*openAuthPage)(getAuthorizeUri(redirectUri));
          // Would be nice to let openAuthPage return a promise,
          // but then we have to add full observable coroutine support, so callback it is
          return self->authorizationCodeChan_.get_observable();
        },
    });

    co_await oauthClient_->run()
        .observe_on(observe_on_asio(*client_->getIoContext()))
        .flat_map([self = shared_from_this()](AuthorizationResult res) {
          if (res) {
            return self->client_->enrollUser(*res);
          } else {
            std::rethrow_exception(res.exception());
          }
        })
        .map([](const EnrollmentResult &) {
          LOG(LOG_TAG, pep::info) << "Completed enrollment!";
          return FakeVoid();
        })
        .finally([self = shared_from_this()] {
          self->oauthClient_.reset();
          self->authorizationCodeChan_ = {};
        });
  }

  /// Call after \c authenticate on successful authentication
  void authenticationCodeCallback(std::string code) {
    LOG(LOG_TAG, debug) << "authenticationCodeCallback";
    if (!oauthClient_) {
      throw std::logic_error("No authentication in progress");
    }
    auto subscriber = authorizationCodeChan_.get_subscriber();
    subscriber.on_next(AuthorizationResult::Success(code));
    subscriber.on_completed();
  }

  /// Call after \c authenticate on authentication failure
  void authenticationErrorCallback(std::string error, std::string errorDescription) {
    LOG(LOG_TAG, debug) << "authenticationErrorCallback";
    if (!oauthClient_) {
      throw std::logic_error("No authentication in progress");
    }
    auto subscriber = authorizationCodeChan_.get_subscriber();
    subscriber.on_next(AuthorizationResult::Failure(std::make_exception_ptr(OAuthError(error, errorDescription))));
    subscriber.on_completed();
  }

  WeblibApiVoidPromise authenticateWithToken(std::string token) {
    co_await onIoThread()
      .flat_map([token = std::move(token)](const std::shared_ptr<Client>& client) {
        return client->enrollUser(token).op(RxInstead(FakeVoid{}));
      });
  }

  /// \returns Promise<string>
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

  /// \returns Promise<ColumnGroup[]>
  WeblibApiPromise listColumns() {
    co_return co_await onIoThread()
        .flat_map([](const std::shared_ptr<Client>& client) {
          return client->getAccessibleColumns(true, { "read" });
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

  /// \returns Promise<SubjectGroup[]>
  WeblibApiPromise listSubjectGroups() {
    co_return co_await onIoThread()
        .flat_map([](const std::shared_ptr<Client>& client) {
          return client->getAccessibleParticipantGroups(true);
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

  /// \returns ReadableStream<CellEntry>
  auto list(weblib::ListQuery query) {
    return CreateReadableStream(
        onIoThread()
        .flat_map([query = MakeSharedCopy(std::move(query))](const std::shared_ptr<Client>& client) {
          auto pps = query->subjects
                     ? client->parsePpsOrIdentities(*query->subjects)
                     .op(RxGetOne("PolymorphicPseudonym vector"))
                     .as_dynamic()
                     : rxcpp::observable<>::just(std::make_shared<std::vector<PolymorphicPseudonym>>())
                     .as_dynamic();

          return pps.flat_map([client, query](const std::shared_ptr<std::vector<PolymorphicPseudonym>>& pps) {
            return client->requestTicket2(requestTicket2Opts{
                .participantGroups = std::move(query->subjectGroups).value_or(std::vector<std::string>{}),
                .pps = std::move(*pps),
                .columnGroups = std::move(query->columnGroups).value_or(std::vector<std::string>{}),
                .columns = std::move(query->columns).value_or(std::vector<std::string>{}),
                .modes = {"read"}, //TODO support more modes
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

  auto retrieve(std::vector<const CellEntry*> entries) {
    return CreateReadableStream(
        onIoThread()
        .flat_map([entries = MakeSharedCopy(std::move(entries))](
            const std::shared_ptr<Client>& client) -> rxcpp::observable<CellData> {
              if (entries->empty()) { return rxcpp::observable<>::empty<CellData>(); }
              auto signedTicket = entries->front()->ticket;
              if (!all_of(*entries, [&signedTicket](const CellEntry* entry) {
                return entry->ticket == signedTicket;
              })) {
                throw std::invalid_argument("Entry tickets differ. Can only retrieve entries from the same list call.");
              }

              auto pageBatches = self->client_->retrieveData(
                  self->client_->getKeys(
                      rxcpp::observable<>::iterate(
                          RangeToVector(*entries | views::transform(std::mem_fn(&CellEntry::inner)))),
                      signedTicket),
                  signedTicket);
              return rxcpp::observable<>::just(entries)
                  .observe_on(observe_on_emscripten(emscripten_main_runtime_thread_id()))
                  .flat_map([pageBatches](const std::shared_ptr<std::vector<const CellEntry*>>& entries) {

                    auto cellStreams = std::make_shared<std::vector<rxcpp::subjects::subject<std::string>>>(entries->size());

                    std::vector<CellData> cellDatas;
                    cellDatas.reserve(entries->size());
                    for (std::size_t cellNum{}; cellNum < entries->size(); ++cellNum) {
                      cellDatas.emplace_back((*entries)[cellNum],
                          CreateReadableByteStream((*cellStreams)[cellNum].get_observable(), 1 << 20 /*1 MiB*/));
                    }

                    pageBatches
                        //TODO Do not request all batches at once, by implementing `pull` with ReadableStream
                        .concat()
                        .observe_on(observe_on_emscripten(emscripten_main_runtime_thread_id()))
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
                              LOG(LOG_TAG, debug) << "Completed ret";
                            });
                    return rxcpp::observable<>::iterate(std::move(cellDatas));
                  });
            })
        );
  }

  WeblibApiPromise registerParticipant(ParticipantPersonalia personalia, bool isTestParticipant) {
    co_return co_await onIoThread()
        .flat_map([personalia = std::move(personalia), isTestParticipant](const std::shared_ptr<Client>& client) {
          return client->registerParticipant(personalia, isTestParticipant);
        });
  }
};

void exitRuntime() { ::emscripten_force_exit(0); }

EMSCRIPTEN_BINDINGS(weblib) {
  class_<Weblib>("Weblib")
      .smart_ptr_constructor("Weblib", &Weblib::Create<>)
      .function("onStatusChange", &Weblib::onStatusChange)
      .function("getEnrolledUser", &Weblib::getEnrolledUser)
      .function("authenticate", &Weblib::authenticate)
      .function("authenticationCodeCallback", &Weblib::authenticationCodeCallback)
      .function("authenticationErrorCallback", &Weblib::authenticationErrorCallback)
      .function("authenticateWithToken", &Weblib::authenticateWithToken)
      .function("internalGenerateToken", &Weblib::internalGenerateToken)
      .function("listColumns", &Weblib::listColumns)
      .function("listSubjectGroups", &Weblib::listSubjectGroups)
      .function("registerParticipant", &Weblib::registerParticipant)
      .function("list", &Weblib::list)
      .function("retrieve", &Weblib::retrieve)
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
          << "Either add a 'caught exception' breakpoint in the debugger or switch to Emscripted EH" << std::endl;
      }
      std::cerr << "Error: " << GetExceptionMessage(std::move(ex)) << std::endl;
    } catch (...) {
      /*ignore: we did what we could*/
    }
    std::abort();
  });
  Logging::Initialize({std::make_shared<ConsoleLogging>(debug /*TODO*/)});

  ::emscripten_exit_with_live_runtime();
}
