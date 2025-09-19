#include <pep/networking/Connection.hpp>

namespace pep::networking {

void Connection::setSocket(std::shared_ptr<Protocol::Socket> socket, SocketConnectivityChangeHandler handleSocketConnectivityChange) {
  assert(socket != nullptr);

  // If we're replacing an existing socket, don't forward the old socket's "disconnecting" and "disconnected" events
  mSocketConnectivityForwarding.cancel();
  this->discardSocket();

  this->setConnectivityStatus(ConnectivityStatus::connecting);
  mSocket = std::move(socket);

  this->setConnectivityStatus(mSocket->status());
  mSocketConnectivityForwarding = mSocket->onConnectivityChange.subscribe(std::move(handleSocketConnectivityChange));
}

void Connection::discardSocket() {
  if (mSocket != nullptr) {
    mSocket->close();
    mSocket = nullptr;
    // Don't cancel mSocketConnectivityForwarding: we may still need to forward the socket's asynchronous "disconnected" notification
  }
}

void Connection::close() {
  mSocketConnectivityForwarding.cancel();
  this->discardSocket();
  if (this->status() < ConnectivityStatus::disconnecting) {
    this->setConnectivityStatus(ConnectivityStatus::disconnecting);
  }
}

std::string Connection::remoteAddress() const {
  if (mSocket == nullptr) {
    throw std::runtime_error("Can't retrieve remote address from a non-connected connection");
  }
  return mSocket->remoteAddress();
}

void Connection::asyncRead(void* destination, size_t bytes, const SizedTransfer::Handler& onTransferred) {
  if (auto socket = this->getSocketOrNotifyTransferFailure(onTransferred)) {
    socket->asyncRead(destination, bytes, onTransferred);
  }
}

void Connection::asyncReadUntil(const char* delimiter, const DelimitedTransfer::Handler& onTransferred) {
  if (auto socket = this->getSocketOrNotifyTransferFailure(onTransferred)) {
    socket->asyncReadUntil(delimiter, onTransferred);
  }
}

void Connection::asyncReadAll(const DelimitedTransfer::Handler& onTransferred) {
  if (auto socket = this->getSocketOrNotifyTransferFailure(onTransferred)) {
    socket->asyncReadAll(onTransferred);
  }
}

void Connection::asyncWrite(const void* source, size_t bytes, const SizedTransfer::Handler& onTransferred) {
  if (auto socket = this->getSocketOrNotifyTransferFailure(onTransferred)) {
    socket->asyncWrite(source, bytes, onTransferred);
  }
}

}
