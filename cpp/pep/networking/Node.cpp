#include <pep/networking/Node.hpp>
#include <pep/utils/Shared.hpp>

namespace pep::networking {

Node::Node(std::shared_ptr<Protocol::NodeComponent> component) noexcept
  : component_(std::move(component)) {
}

void Node::shutdown() {
  if (this->status() != Status::Uninitialized && this->status() < Status::Finalizing) {
    this->setStatus(Status::Finalizing);
  }

  // Don't iterate directly over sockets_ because closing each socket will remove it from sockets_, invalidating our iterator
  auto all = std::move(sockets_);
  sockets_.clear(); // Just in case our std::move didn't clear sockets_

  for (auto& socket : all) {
    socket.first->close();
  }

  // Discard our component _after_ the sockets, which may need the component to clean up after themselves
  if (component_ != nullptr) {
    component_->close();
    component_.reset();
  }

  this->setStatus(Status::Finalized); // TODO: don't notify until sockets and component have been Status::finalized
}

void Node::openSocket(const SocketConnectionAttempt::Handler& onSocketConnection) {
  assert(component_ != nullptr);

  auto weak = WeakFrom(*this);

  auto socket = component_->openSocket([weak, onSocketConnection](const SocketConnectionAttempt::Result& socketResult) {
    if (!socketResult) { // Socked couldnt' be opened: forward the error to our callback
      onSocketConnection(socketResult);
      return;
    }

    // The socket opened OK
    auto socket = *socketResult;
    assert(socket != nullptr);
    auto self = weak.lock();
    if (self == nullptr) { // This Node has been (or is being) destroyed
      socket->close(); // Get rid of the resource that we won't be managing
      onSocketConnection(SocketConnectionAttempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Node was destroyed"))));
      return;
    }

    // Notify external listener that we've established connectivity
    onSocketConnection(socketResult);
    });

  // Discard our (shared_)ptr to the socket when it gets closed
  auto subscription = socket->onConnectivityChange.subscribe([weak, socket](const Protocol::Socket::ConnectivityChange& change) {
    auto self = weak.lock();
    if (change.updated >= Transport::ConnectivityStatus::Disconnecting && self != nullptr) {
      self->sockets_.erase(socket);
    }
    });

  // Store the socket so we can close it when the node is shut down
  [[maybe_unused]] auto emplaced = sockets_.emplace(socket, std::move(subscription)).second;
  assert(emplaced);
}

void Node::handleConnectionAttempt(const Connection::Attempt::Result& status) const {
  onConnectionAttempt.notify(status);
}

void Node::start() {
  auto status = this->status();
  if (status > Status::Initialized) {
    throw std::runtime_error("Can't start a node that has been shut down");
  }
  if (status != Status::Uninitialized) {
    throw std::runtime_error("Can't start a node more than once");
  }

  this->setStatus(Status::Initializing);
  this->setStatus(Status::Initialized);
  this->establishConnection();
}

}
