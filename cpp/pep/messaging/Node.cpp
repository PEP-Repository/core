#include <pep/async/CreateObservable.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/messaging/ConnectionFailureException.hpp>
#include <pep/networking/Server.hpp>
#include <pep/utils/Log.hpp>

namespace pep::messaging {

namespace {

const std::string LogTag = "Messaging node";

std::string GetIncompatibleVersionSummary(const std::optional<GitlabVersion>& version) {
  if (version == std::nullopt) {
    return "<unspecified>";
  }
  auto result = version->getSummary();
  if (result.empty()) {
    return "<empty>";
  }
  return result;
}

void LogIncompatibleVersionDetails(Severity severity, const std::string& type, const std::optional<GitlabVersion>& remote, const std::optional<GitlabVersion>& local) {
  if (remote != std::nullopt || local != std::nullopt) {
    PEP_LOG(LogTag, severity) << "- " << type << " versions"
      << ": remote = " << GetIncompatibleVersionSummary(remote)
      << "; local = " << GetIncompatibleVersionSummary(local);
  }
}

class BinaryFinalizationNotifier {
private:
  EventSubscription subscription_;
  std::optional<rxcpp::subscriber<FakeVoid>> subscriber_;

private:
  BinaryFinalizationNotifier() noexcept = default;

  void notify() {
    if (subscriber_.has_value()) {
      subscriber_->on_next(FakeVoid());
      subscriber_->on_completed();
      subscriber_.reset();
    }
    subscription_.cancel();
  }

public:
  static std::shared_ptr<BinaryFinalizationNotifier> Create(const networking::Node& node) {
    std::shared_ptr<BinaryFinalizationNotifier> result(new BinaryFinalizationNotifier());

    if (node.status() != LifeCycler::Status::Finalized) { // Don't subscribe (because event won't be notified) if the node is already finalized
      result->subscription_ = node.onStatusChange.subscribe([result](const LifeCycler::StatusChange& change) {
        if (change.updated == LifeCycler::Status::Finalized) {
          result->notify();
        }
        });
    }

    return result;
  }

  void hookup(rxcpp::subscriber<FakeVoid> subscriber) {
    assert(!subscriber_.has_value());
    subscriber_ = subscriber;

    if (!subscription_.active()) { // The node was finalized to begin with
      this->notify();
    }
    // else wait for us to be notified through our subscription_
  }
};
}

Node::Node(const networking::Protocol::ServerParameters& parameters, RequestHandler& requestHandler)
  : ioContext_(parameters.ioContext()), binary_(networking::Server::Create(parameters)), requestHandler_(&requestHandler), incompatibleRemotes_(std::set<IncompatibleRemote>()) {
  assert(binary_->status() == LifeCycler::Status::Uninitialized);
}

Node::Node(const networking::Protocol::ClientParameters& parameters, std::optional<networking::Client::ReconnectParameters> reconnectParameters)
  : ioContext_(parameters.ioContext()), reconnectParameters_(reconnectParameters), binary_(networking::Client::Create(parameters, reconnectParameters_)) {
  assert(binary_->status() == LifeCycler::Status::Uninitialized);
}

void Node::vetConnectionWith(const std::string& description, const std::string& address, const BinaryVersion& binary, const std::optional<ConfigVersion>& config) {
  if (BinaryVersion::current.getProtocolChecksum() != binary.getProtocolChecksum()) {
    auto refuse = binary.isGitlabBuild() && BinaryVersion::current.isGitlabBuild(); // TODO: perhaps make this depend on ConfigVersion::getReference() == "local"?

    std::string msg;
    Severity severity{};
    if (refuse) {
      msg = "Rejected: " + description + " refusing";
      severity = Severity::Error;
    } else {
      msg = "Development genuflection: " + description + " allowing";
      severity = Severity::Warning;
    }

    msg += " connection between incompatible remote (" + binary.getProtocolChecksum() + " at " + address
      + ") and local (" + BinaryVersion::current.getProtocolChecksum() + ") software versions";

    // Always log if we're not keeping track of incompatible remotes (i.e. this is a client node)
    auto log = !incompatibleRemotes_.has_value();
    if (!log) {
      // If we haven't seen this incompatible remote before, log about it now (once)
      IncompatibleRemote remote{ address, GetIncompatibleVersionSummary(binary), GetIncompatibleVersionSummary(config) };
      log = incompatibleRemotes_->emplace(std::move(remote)).second;
    }

    if (log) {
      PEP_LOG(LogTag, severity) << msg;
      LogIncompatibleVersionDetails(severity, "binary", binary, BinaryVersion::current); // TODO: check if derived (BinaryVersion) details are logged instead of only base (GitlabVersion) details
      LogIncompatibleVersionDetails(severity, "config", config, ConfigVersion::Current()); // TODO: check if derived (ConfigVersion) details are logged instead of only base (GitlabVersion) details
    }

    if (refuse) {
      throw ConnectionFailureException::ForVersionCheckFailure(msg);
    }
  }
}

void Node::handleConnectionEstablishing(std::shared_ptr<Connection> connection, const LifeCycler::StatusChange& change) {
  auto existing = std::find_if(existingConnections_.begin(), existingConnections_.end(), [connection](const ExistingConnection& candidate) {
    return candidate.own == connection;
    });
  if (existing == existingConnections_.end()) {
    // This messaging::Connection is sending a notification but we've already discarded it from our existingConnections_, i.e. the associated
    // (binary) networking::Connection has already been destroyed, and we've already run the cleanup code in (the lambda in) Node::start.
    // See https://gitlab.pep.cs.ru.nl/pep/core/-/work_items/2867#note_58687
    // Since this Node (created and therefore) still owns the messaging::Connection, we need to notify our subscriber of the failure.
    assert(change.updated >= LifeCycler::Status::Finalizing && "Messaging connection doing stuff after its binary connection has died");
    if (subscriber_.has_value()) {
      subscriber_->on_next(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Binary connection was destroyed"))));
    }
    return;
  }

  switch (change.updated) {
  case LifeCycler::Status::Reinitializing: // Notify subscriber of our (failed) attempt and retry
    PEP_LOG(LogTag, Severity::Debug) << "Messaging connection reinitializing";
    if (subscriber_.has_value()) {
      subscriber_->on_next(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Failed to establish messaging connection: will be retried"))));
    }
    break;
  case LifeCycler::Status::Initialized: // Established: hand off to subscriber
    PEP_LOG(LogTag, Severity::Debug) << "Messaging connection established";
    existing->establishing.cancel();
    existing->own.reset();
    if (subscriber_.has_value()) {
      subscriber_->on_next(Connection::Attempt::Result::Success(connection));
    }
    break;
  default:
    // ignore
    break;
  }
}

Node::~Node() noexcept {
  this->shutdown();
}

std::string Node::describe() const {
  if (binary_ == nullptr) {
    throw std::runtime_error("Can't retrieve description from discarded networking::Node");
  }
  return binary_->describe();
}

rxcpp::observable<Connection::Attempt::Result> Node::start() {
  assert(subscriber_ == std::nullopt);
  assert(binary_ != nullptr);

  return CreateObservable<Connection::Attempt::Result>([weak = WeakFrom(*this)](rxcpp::subscriber<Connection::Attempt::Result> subscriber) {
    auto self = weak.lock();
    if (self == nullptr) {
      if (subscriber.is_subscribed()) {
        subscriber.on_completed();
        subscriber.unsubscribe();
      }
      return;
    }

    self->subscriber_ = subscriber;
    self->binaryConnectionAttempt_ = self->binary_->onConnectionAttempt.subscribe([weak, subscriber](const networking::Connection::Attempt::Result& binaryResult) {
      if (!binaryResult) {
        subscriber.on_next(Connection::Attempt::Result::Failure(binaryResult.exception()));
      } else {
        auto self = weak.lock();
        if (self == nullptr) {
          if (subscriber.is_subscribed()) {
            subscriber.on_next(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Binary connection established after messaging node was discarded"))));
            subscriber.on_completed();
            subscriber.unsubscribe();
          }
          return;
        }

        std::erase_if(self->existingConnections_, [](const ExistingConnection& candidate) {return candidate.binary.lock() == nullptr; });
        auto binaryConnection = *binaryResult;
        if (std::any_of(self->existingConnections_.begin(), self->existingConnections_.end(), [binaryConnection](const ExistingConnection& existing) {
          return existing.binary.lock() == binaryConnection;
          })) {
          throw std::runtime_error("Node attempting to create a second messaging connection for a single binary connection");
        }

        ExistingConnection existing{
          .binary = binaryConnection,
          .own = Connection::Open(self, binaryConnection, self->ioContext_, self->requestHandler_),
          .establishing{} // Silence clang's -Wmissing-field-initializers
        };
        existing.establishing = existing.own->onStatusChange.subscribe([weak, own = existing.own](const LifeCycler::StatusChange& change) {
          if (auto self = weak.lock()) {
            self->handleConnectionEstablishing(own, change);
          }
          });

        self->existingConnections_.emplace_back(std::move(existing));
      }
      });

    auto binaryFinalization = std::make_shared<EventSubscription>();
    *binaryFinalization = self->binary_->onStatusChange.subscribe([weak, subscriber, binaryFinalization](const LifeCycler::StatusChange& change) {
      if (change.updated == LifeCycler::Status::Finalizing) {
        binaryFinalization->cancel();
        if (subscriber.is_subscribed()) {
          subscriber.on_completed();
          subscriber.unsubscribe();
        }
        if (auto self = weak.lock()) {
          self->subscriber_.reset();
        }
      }
      });

    self->binary_->start();
    });
}

rxcpp::observable<FakeVoid> Node::shutdown() {
  auto binary = std::move(binary_); // Ensure that a (possibly recursive) next call is a no-op
  if (binary == nullptr) {
    return rxcpp::observable<>::just(FakeVoid());
  }

  binaryConnectionAttempt_.cancel();
  for (auto& existing : existingConnections_) {
    existing.establishing.cancel();
    existing.own.reset();
  }

  auto notifier = BinaryFinalizationNotifier::Create(*binary);
  binary->shutdown();
  return CreateObservable<FakeVoid>([notifier](rxcpp::subscriber<FakeVoid> subscriber) {
    notifier->hookup(subscriber);
    });
}

}
