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

Node::Node(boost::asio::io_context& ioContext, std::shared_ptr<networking::Server> binary, RequestHandler& requestHandler)
  : mIoContext(ioContext), mBinary(std::move(binary)), mRequestHandler(&requestHandler), mIncompatibleRemotes(std::set<IncompatibleRemote>()) {
  assert(mBinary->status() == LifeCycler::Status::uninitialized);
}

Node::Node(boost::asio::io_context& ioContext, std::shared_ptr<networking::Client> binary)
  : mIoContext(ioContext), mBinary(std::move(binary)) {
  assert(mBinary->status() == LifeCycler::Status::uninitialized);
}

Node::Node(const networking::Protocol::ServerParameters& parameters, RequestHandler& requestHandler)
  : Node(parameters.ioContext(), networking::Server::Create(parameters), requestHandler) {
}

Node::Node(const networking::Protocol::ClientParameters& parameters, std::optional<networking::Client::ReconnectParameters> reconnectParameters)
  : Node(parameters.ioContext(), networking::Client::Create(parameters, reconnectParameters)) {
}

void Node::vetConnectionWith(const std::string& description, const std::string& address, const BinaryVersion& binary, const std::optional<ConfigVersion>& config) {
  if (BinaryVersion::current.getProtocolChecksum() != binary.getProtocolChecksum()) {
    auto refuse = binary.isGitlabBuild() && BinaryVersion::current.isGitlabBuild(); // TODO: perhaps make this depend on ConfigVersion::getReference() == "local"?

    std::string msg;
    severity_level severity;
    if (refuse) {
      msg = "Refusing";
      severity = error;
    } else {
      msg = "Development genuflection: allowing";
      severity = warning;
    }

    msg += " connection between incompatible remote " + description + " (" + binary.getProtocolChecksum()
      + " at " + address
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

        std::erase_if(self->mExistingConnections, [](std::weak_ptr<networking::Connection> candidate) {return candidate.lock() == nullptr; });
        auto binaryConnection = *binaryResult;
        if (std::any_of(self->mExistingConnections.begin(), self->mExistingConnections.end(), [binaryConnection](std::weak_ptr<networking::Connection> existing) {
          return existing.lock() == binaryConnection;
          })) {
          throw std::runtime_error("Node attempting to create a second messaging connection for a single binary connection");
        }
        self->mExistingConnections.push_back(binaryConnection);

        Connection::Open(self, binaryConnection, self->mIoContext, self->mRequestHandler, [subscriber](Connection::Attempt::Result result) {subscriber.on_next(std::move(result)); });
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

  auto notifier = BinaryFinalizationNotifier::Create(*binary);
  binary->shutdown();
  return CreateObservable<FakeVoid>([notifier](rxcpp::subscriber<FakeVoid> subscriber) {
    notifier->hookup(subscriber);
    });
}

}
