#include <pep/messaging/BinaryProtocol.hpp>
#include <pep/messaging/ServerConnection.hpp>

namespace pep::messaging {

ServerConnection::ServerConnection(std::shared_ptr<Node> node) noexcept
  : mNode(node), mWaitGroup(WaitGroup::Create()) {
  assert(mNode != nullptr);
  this->onDisconnected();
}

void ServerConnection::handleConnectivityChange(const LifeCycler::StatusChange& change) {
  if (change.updated == LifeCycler::Status::initialized) {
    this->handleConnectivityChange(Connection::Attempt::Result::Success(mConnection));
  } else {
    this->handleConnectivityChange(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Connectivity lost")))); // TODO: get reason from mConnection or "change" parameter
  }
}

void ServerConnection::handleConnectivityChange(const Connection::Attempt::Result& result) {
  ConnectionStatus status;

  if (result) {
    this->onConnected(*result);
    // status already indicates connected/success
  } else {
    status.connected = false;
    try {
      std::rethrow_exception(result.exception());
    }
    catch (const boost::system::system_error& error) {
      status.error = error.code();
    }
    catch (...) {
      status.error = boost::system::errc::make_error_code(boost::system::errc::errc_t::not_connected);
    }
    this->onDisconnected();
  }

  mStatusSubscriber.on_next(status);
}

void ServerConnection::onConnected(std::shared_ptr<Connection> connection) {
  assert(mConnection == nullptr || mConnection == connection);

  if (mConnection == nullptr) {
    mConnection = connection;
    mConnectionStatusSubscription = mConnection->onStatusChange.subscribe([weak = WeakFrom(*this)](const LifeCycler::StatusChange& change) {
      auto self = weak.lock();
      if (self != nullptr) {
        self->handleConnectivityChange(change);
      }
      });
  }

  assert(mConnecting.has_value());
  mConnecting->done();
  mConnecting.reset();
  mWaitGroup = WaitGroup::Create();
}

void ServerConnection::onDisconnected() {
  if (!mConnecting.has_value()) {
    mWaitGroup = WaitGroup::Create();
    mConnecting.emplace(mWaitGroup->add("Connecting"));
  }
}

void ServerConnection::finalize() {
  if (mNode != nullptr) {
    mNode->shutdown();
    mNode.reset();
  }
  mConnection.reset();
}

void ServerConnection::handleConnectivityError(std::exception_ptr error) {
  this->finalize();
  mStatusSubscriber.on_error(error);
}

void ServerConnection::handleConnectivityEnd() {
  this->finalize();
  mStatusSubscriber.on_completed();
}

std::shared_ptr<ServerConnection> ServerConnection::Create(std::shared_ptr<boost::asio::io_context> io_context, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath) {
  if (endPoint.hostname.empty()) {
    throw std::runtime_error("Can't establish a server connection if host name isn't specified");
  }

  auto binaryParameters = BinaryProtocol::CreateClientParameters(*io_context, endPoint, caCertFilepath);
  auto node = Node::Create(*binaryParameters);
  auto connection = std::shared_ptr<ServerConnection>(new ServerConnection(node));
  node->start().subscribe(
    [connection](const Connection::Attempt::Result& result) { connection->handleConnectivityChange(result); },
    [connection](std::exception_ptr error) { connection->handleConnectivityError(error); },
    [connection]() { connection->handleConnectivityEnd(); });

  return connection;

}

std::shared_ptr<ServerConnection> ServerConnection::TryCreate
(std::shared_ptr<boost::asio::io_context> io_context, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath) {
  if (endPoint.hostname.empty()) {
    return nullptr;
  }
  return Create(io_context, endPoint, caCertFilepath);
}

rxcpp::observable<ConnectionStatus> ServerConnection::connectionStatus() {
  return mStatus.get_observable();
}

rxcpp::observable<std::string> ServerConnection::sendRequest(std::shared_ptr<std::string> message, std::optional<messaging::MessageBatches> tail) {
  return this->whenConnected<std::string>([message, tail](std::shared_ptr<Connection> connection) {
    return connection->sendRequest(message, tail);
    });
  
}

rxcpp::observable<FakeVoid> ServerConnection::shutdown() {
  if (mNode == nullptr) {
    return rxcpp::observable<>::just(FakeVoid());
  }
  return mNode->shutdown();
}

}
