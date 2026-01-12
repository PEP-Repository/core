#include <pep/async/CreateObservable.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/messaging/ConnectionFailureException.hpp>
#include <pep/networking/Server.hpp>
#include <pep/utils/Log.hpp>

namespace pep::messaging {

namespace {

const std::string LOG_TAG = "Messaging node";

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

void LogIncompatibleVersionDetails(severity_level severity, const std::string& type, const std::optional<GitlabVersion>& remote, const std::optional<GitlabVersion>& local) {
  if (remote != std::nullopt || local != std::nullopt) {
    LOG(LOG_TAG, severity) << "- " << type << " versions"
      << ": remote = " << GetIncompatibleVersionSummary(remote)
      << "; local = " << GetIncompatibleVersionSummary(local);
  }
}

class BinaryFinalizationNotifier {
private:
  EventSubscription mSubscription;
  std::optional<rxcpp::subscriber<FakeVoid>> mSubscriber;

private:
  BinaryFinalizationNotifier() noexcept = default;

  void notify() {
    if (mSubscriber.has_value()) {
      mSubscriber->on_next(FakeVoid());
      mSubscriber->on_completed();
      mSubscriber.reset();
    }
    mSubscription.cancel();
  }

public:
  static std::shared_ptr<BinaryFinalizationNotifier> Create(const networking::Node& node) {
    std::shared_ptr<BinaryFinalizationNotifier> result(new BinaryFinalizationNotifier());

    if (node.status() != LifeCycler::Status::finalized) { // Don't subscribe (because event won't be notified) if the node is already finalized
      result->mSubscription = node.onStatusChange.subscribe([result](const LifeCycler::StatusChange& change) {
        if (change.updated == LifeCycler::Status::finalized) {
          result->notify();
        }
        });
    }

    return result;
  }

  void hookup(rxcpp::subscriber<FakeVoid> subscriber) {
    assert(!mSubscriber.has_value());
    mSubscriber = subscriber;

    if (!mSubscription.active()) { // The node was finalized to begin with
      this->notify();
    }
    // else wait for us to be notified through our mSubscription
  }
};
}

Node::Node(const networking::Protocol::ServerParameters& parameters, RequestHandler& requestHandler)
  : mIoContext(parameters.ioContext()), mBinary(networking::Server::Create(parameters)), mRequestHandler(&requestHandler), mIncompatibleRemotes(std::set<IncompatibleRemote>()) {
  assert(mBinary->status() == LifeCycler::Status::uninitialized);
}

Node::Node(const networking::Protocol::ClientParameters& parameters, std::optional<networking::Client::ReconnectParameters> reconnectParameters)
  : mIoContext(parameters.ioContext()), mBinary(networking::Client::Create(parameters, std::move(reconnectParameters))) {
  assert(mBinary->status() == LifeCycler::Status::uninitialized);
}

void Node::vetConnectionWith(const std::string& description, const std::string& address, const BinaryVersion& binary, const std::optional<ConfigVersion>& config) {
  if (BinaryVersion::current.getProtocolChecksum() != binary.getProtocolChecksum()) {
    auto refuse = binary.isGitlabBuild() && BinaryVersion::current.isGitlabBuild(); // TODO: perhaps make this depend on ConfigVersion::getReference() == "local"?

    std::string msg;
    severity_level severity;
    if (refuse) {
      msg = "Rejected: " + description + " refusing";
      severity = error;
    } else {
      msg = "Development genuflection: " + description + " allowing";
      severity = warning;
    }

    msg += " connection between incompatible remote (" + binary.getProtocolChecksum() + " at " + address
      + ") and local (" + BinaryVersion::current.getProtocolChecksum() + ") software versions";

    // Always log if we're not keeping track of incompatible remotes (i.e. this is a client node)
    auto log = !mIncompatibleRemotes.has_value();
    if (!log) {
      // If we haven't seen this incompatible remote before, log about it now (once)
      IncompatibleRemote remote{ address, GetIncompatibleVersionSummary(binary), GetIncompatibleVersionSummary(config) };
      log = mIncompatibleRemotes->emplace(std::move(remote)).second;
    }

    if (log) {
      LOG(LOG_TAG, severity) << msg;
      LogIncompatibleVersionDetails(severity, "binary", binary, BinaryVersion::current); // TODO: check if derived (BinaryVersion) details are logged instead of only base (GitlabVersion) details
      LogIncompatibleVersionDetails(severity, "config", config, ConfigVersion::Current()); // TODO: check if derived (ConfigVersion) details are logged instead of only base (GitlabVersion) details
    }

    if (refuse) {
      throw ConnectionFailureException::ForVersionCheckFailure(msg);
    }
  }
}

void Node::handleConnectionEstablishing(std::shared_ptr<Connection> connection, const LifeCycler::StatusChange& change) {
  auto existing = std::find_if(mExistingConnections.begin(), mExistingConnections.end(), [connection](const ExistingConnection& candidate) {
    return candidate.own == connection;
    });
  assert(existing != mExistingConnections.end());

  switch (change.updated) {
  case LifeCycler::Status::reinitializing: // Notify subscriber of our (failed) attempt and retry
    LOG(LOG_TAG, debug) << "Messaging connection reinitializing";
    if (mSubscriber.has_value()) {
      mSubscriber->on_next(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Failed to establish messaging connection: will be retried"))));
    }
    // TODO: prevent immediate retry: use ExponentialBackoff
    break;
  case LifeCycler::Status::initialized: // Established: hand off to subscriber
    LOG(LOG_TAG, debug) << "Messaging connection established";
    existing->establishing.cancel();
    existing->own.reset();
    if (mSubscriber.has_value()) {
      mSubscriber->on_next(Connection::Attempt::Result::Success(connection));
    }
    break;
  }
}

Node::~Node() noexcept {
  this->shutdown();
}

std::string Node::describe() const {
  if (mBinary == nullptr) {
    throw std::runtime_error("Can't retrieve description from discarded networking::Node");
  }
  return mBinary->describe();
}

rxcpp::observable<Connection::Attempt::Result> Node::start() {
  assert(mSubscriber == std::nullopt);
  assert(mBinary != nullptr);

  return CreateObservable<Connection::Attempt::Result>([weak = WeakFrom(*this)](rxcpp::subscriber<Connection::Attempt::Result> subscriber) {
    auto self = weak.lock();
    if (self == nullptr) {
      if (subscriber.is_subscribed()) {
        subscriber.on_completed();
        subscriber.unsubscribe();
      }
      return;
    }

    self->mSubscriber = subscriber;
    self->mBinaryConnectionAttempt = self->mBinary->onConnectionAttempt.subscribe([weak, subscriber](const networking::Connection::Attempt::Result& binaryResult) {
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

        std::erase_if(self->mExistingConnections, [](const ExistingConnection& candidate) {return candidate.binary.lock() == nullptr; });
        auto binaryConnection = *binaryResult;
        if (std::any_of(self->mExistingConnections.begin(), self->mExistingConnections.end(), [binaryConnection](const ExistingConnection& existing) {
          return existing.binary.lock() == binaryConnection;
          })) {
          throw std::runtime_error("Node attempting to create a second messaging connection for a single binary connection");
        }

        ExistingConnection existing{
          .binary = binaryConnection,
          .own = Connection::Open(self, binaryConnection, self->mIoContext, self->mRequestHandler),
        };
        existing.establishing = existing.own->onStatusChange.subscribe([weak, own = existing.own](const LifeCycler::StatusChange& change) {
          if (auto self = weak.lock()) {
            self->handleConnectionEstablishing(own, change);
          }
          });

        self->mExistingConnections.emplace_back(std::move(existing));
      }
      });

    auto binaryFinalization = std::make_shared<EventSubscription>();
    *binaryFinalization = self->mBinary->onStatusChange.subscribe([subscriber, binaryFinalization](const LifeCycler::StatusChange& change) {
      if (change.updated == LifeCycler::Status::finalizing) {
        binaryFinalization->cancel();
        if (subscriber.is_subscribed()) {
          subscriber.on_completed();
          subscriber.unsubscribe();
        }
      }
      });

    self->mBinary->start();
    });
}

rxcpp::observable<FakeVoid> Node::shutdown() {
  auto binary = std::move(mBinary); // Ensure that a (possibly recursive) next call is a no-op
  if (binary == nullptr) {
    return rxcpp::observable<>::just(FakeVoid());
  }

  mBinaryConnectionAttempt.cancel();
  for (auto& existing : mExistingConnections) {
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
