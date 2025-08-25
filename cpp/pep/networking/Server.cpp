#include <pep/networking/Connection.hpp>
#include <pep/networking/Server.hpp>
#include <pep/utils/Defer.hpp>

namespace pep::networking {

class Server::Connection : public networking::Connection {
public:
  explicit Connection() = default;

  using networking::Connection::setSocket;
  using networking::Connection::setConnectivityStatus;
};


bool Server::acceptNewClient() {
  if (this->isRunning()) {
    this->establishConnection();
    return true;
  }
  return false;
}

void Server::establishConnection() {
  assert(this->isRunning());

  this->openSocket([weak = WeakFrom(*this)](const SocketConnectionAttempt::Result& socketResult) {
    auto self = weak.lock();
    if (self == nullptr) { // This Server has been (or is being) destroyed
      if (socketResult) {
        (*socketResult)->close();
      }
      // We can't invoke self->onConnectionAttempt
      return;
    }

    if (!socketResult) { // Socked couldnt' be opened
      if (self->acceptNewClient()) { // We're not shutting down: forward the failure to calling code
        self->handleConnectionAttempt(Connection::Attempt::Result::Failure(socketResult.exception()));
      }
      return;
    }

    auto socket = *socketResult;
    assert(socket != nullptr);

    if (!self->acceptNewClient()) {
      socket->close();
      self->handleConnectionAttempt(Connection::Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Server was stopped"))));
      return;
    }

    // Create connection and hook up onConnectivityChange event
    auto connection = std::make_shared<Connection>();
    connection->setSocket(socket, [weak = std::weak_ptr<Connection>(connection)](const Connection::ConnectivityChange& change) {
      auto self = weak.lock();
      if (self != nullptr) {
        self->setConnectivityStatus(change.updated);
      }
      });

    // Notify external listener
    self->handleConnectionAttempt(Connection::Attempt::Result::Success(connection));
    });
}

std::shared_ptr<Protocol::ClientParameters> Server::createClientParameters() const {
  assert(mComponent != nullptr);
  return mComponent->createClientParameters();
}

void Server::shutdown() {
  if (this->status() != Status::uninitialized && this->status() < Status::finalizing) {
    this->setStatus(Status::finalizing);
  }
  mComponent.reset(); // Base (Node) class will invoke its close method
  Node::shutdown();
}

}
