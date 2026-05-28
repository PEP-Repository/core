#include <pep/async/CreateObservable.hpp>
#include <pep/messaging/BinaryProtocol.hpp>
#include <pep/messaging/ServerConnection.hpp>
#include <pep/utils/MiscUtil.hpp>

namespace pep::messaging {

ServerConnection::ServerConnection(std::shared_ptr<Node> node) noexcept
  : mNode(node) {
  assert(mNode != nullptr);
}

void ServerConnection::handleConnectivityChange(const LifeCycler::StatusChange& change) {
  if (change.updated == LifeCycler::Status::Initialized) {
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

    // Send pending requests now
    auto send = std::exchange(mPendingRequests, Default<decltype(mPendingRequests)>);
    for (const auto& request: send) {
      rxcpp::observable<std::string> obs;
      try {
        obs = mConnection->sendRequest(request.message, request.tail);
      }
      catch (...) { // Notify subscriber that request can't be sent
        request.subscriber.on_error(std::current_exception());
        continue; // ... then continue with the next pending request
      }

      // Request has been scheduled onto the mConnection: hook up the subscriber
      obs.subscribe(request.subscriber);
    }
  }
}

void ServerConnection::finalize() {
  this->shutdown();
  mNode.reset();
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
  if (mConnection != nullptr) {
    return mConnection->sendRequest(message, tail);
  }

  return CreateObservable<std::string>([weak = WeakFrom(*this), message, tail](rxcpp::subscriber<std::string> subscriber) {
      auto self = weak.lock();
      if (self == nullptr) {
        subscriber.on_error(std::make_exception_ptr(std::runtime_error("Server connection was destroyed")));
      }
      else if (self->mConnection != nullptr) { // Connection has been established before caller subscribed
        self->mConnection->sendRequest(message, tail).subscribe(subscriber);
      }
      else { // Connection has not been established yet
        self->mPendingRequests.emplace_back(PendingRequest{ // Store the request so it can be sent when the connection is established later
          .message = message,
          .tail = tail,
          .subscriber = subscriber,
          });
      }
    });
}

rxcpp::observable<FakeVoid> ServerConnection::shutdown() {
  if (mNode == nullptr) {
    return rxcpp::observable<>::just(FakeVoid());
  }

  auto result = mNode->shutdown();
  auto pending = std::move(mPendingRequests);
  mPendingRequests.clear(); // Our call to std::move doesn't (necessarily) clear the vector
  for (const auto& request : pending) {
    request.subscriber.on_error(std::make_exception_ptr(std::runtime_error("Server connection is shutting down")));
  }
  return result;
}

}
