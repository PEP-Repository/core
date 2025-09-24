#include <pep/networking/TcpBasedProtocol.hpp>
#include <pep/utils/Defer.hpp>
#include <boost/asio/connect.hpp>
#include <pep/utils/Log.hpp>

namespace pep::networking {

void TcpBasedProtocol::Socket::raiseTransferStartFailure(size_t& pending, size_t pend, const std::string& reason) const {
  assert(&pending == &mPendingReadBytes || &pending == &mPendingWriteBytes);
  auto type = (&pending == &mPendingReadBytes) ? "read" : "write";
  throw std::runtime_error("Can't start a new " + std::to_string(pend) + "-byte " + type + " action " + reason);
}

void TcpBasedProtocol::Socket::startTransfer(size_t& pending, size_t pend) {
  assert(pend != 0U);

  if (!this->isConnected()) {
    raiseTransferStartFailure(pending, pend, "on a socket that's not connected");
  }
  if (pending != 0U) {
    raiseTransferStartFailure(pending, pend, "before the previous " + std::to_string(pending) + "-byte one is finished");
  }

  pending = pend;
}

void TcpBasedProtocol::Socket::finishConnecting(const ConnectionAttempt::Handler& notify) {
  assert(this->status() == ConnectivityStatus::connecting);
  this->setConnectivityStatus(ConnectivityStatus::connected);
  notify(ConnectionAttempt::Result::Success(SharedFrom(*this)));
}

void TcpBasedProtocol::Socket::onTransferComplete(size_t& pendingBytes, const boost::system::error_code& error, size_t transferred) {
  if (error) {
    this->close();
  }
  else {
    assert(pendingBytes == transferred);
    pendingBytes = 0U;
  }
}

TcpBasedProtocol::Socket::Socket(const TcpBasedProtocol& protocol, boost::asio::io_context& ioContext)
  : Protocol::Socket(protocol, ioContext), TcpBound(protocol), mReadBuffer(SocketReadBuffer::Create()) {
}

std::string TcpBasedProtocol::Socket::remoteAddress() const {
  try {
    return this->basicSocket().remote_endpoint().address().to_string();
  }
  catch (const boost::system::system_error& ex) { // remote_endpoint may throw, e.g. when not connected
    using namespace std::literals;
    // Simplify message (probably just disconnected)
    auto code = ex.code();
    boost::system::error_code withoutLocation(code.value(), code.category());
    return "[error: "s + withoutLocation.what() + "]";
  }
}

void TcpBasedProtocol::Socket::asyncRead(void* destination, size_t bytes, const SizedTransfer::Handler& onTransferred) {
  this->startTransfer(mPendingReadBytes, bytes);

  mReadBuffer->asyncRead(this->streamSocket(), destination, bytes, [self = SharedFrom(*this), onTransferred](const boost::system::error_code& error, size_t bytes) {
    self->onTransferComplete(self->mPendingReadBytes, error, bytes);
    onTransferred(BoostOperationResult(error, bytes));
    });
}

void TcpBasedProtocol::Socket::asyncReadUntil(const char* delimiter, const DelimitedTransfer::Handler& onTransferred) {
  assert(delimiter != nullptr);
  auto delimiterSize = strlen(delimiter);
  this->startTransfer(mPendingReadBytes, delimiterSize); // We're going to read (at least) the delimiter's number of bytes

  mReadBuffer->asyncReadUntil(this->streamSocket(),
    delimiter,
    [self = SharedFrom(*this), delimiterSize, onTransferred](const boost::system::error_code& error, const std::string& result) {
      self->onTransferComplete(self->mPendingReadBytes, error, delimiterSize);
      onTransferred(BoostOperationResult(error, result));
    });
}

void TcpBasedProtocol::Socket::asyncReadAll(const DelimitedTransfer::Handler& onTransferred) {
  this->startTransfer(mPendingReadBytes, 1U); // We don't know how much we're going to read, but we need to indicate that a (non-zero) read is pending
  mReadBuffer->asyncReadAll(this->streamSocket(),
    [self = SharedFrom(*this), onTransferred](const boost::system::error_code& error, const std::string& result) {
      if (error) {
        // Normal sequence of events: update own state before notifying caller of the failure
        self->onTransferComplete(self->mPendingReadBytes, error, 1U); // Closes the socket
        onTransferred(BoostOperationResult(error, result));
      }
      else {
        // We've read until EOF, so the remote party has disconnected and we'll need to close this socket.
        // But we want to notify our callback of the read action's success _before_ that time, so that the
        // caller gets the result of their (successful) read action before they can see the socket closing.
        // We therefore reverse the normal sequence of events, notifying the caller _before_ updating our
        // own state. If the caller tries to schedule a new (read or write) transfer from the callback,
        // it'll get a "can't start a new [...] action" exception.
        onTransferred(BoostOperationResult(error, result));
        self->onTransferComplete(self->mPendingReadBytes, error, 1U);
        self->close();
      }
    });
}

void TcpBasedProtocol::Socket::asyncWrite(const void* source, size_t bytes, const SizedTransfer::Handler& onTransferred) {
  this->startTransfer(mPendingWriteBytes, bytes);

  this->streamSocket().asyncWrite(source, bytes, [self = SharedFrom(*this), onTransferred](const boost::system::error_code& error, size_t bytes) {
    self->onTransferComplete(self->mPendingWriteBytes, error, bytes);
    onTransferred(BoostOperationResult(error, bytes));
    });
}

TcpBasedProtocol::ClientComponent::ClientComponent(const ClientParameters& parameters)
  : Protocol::ClientComponent(parameters)
  , TcpBound(parameters.tcp())
  , mEndPoint(parameters.endPoint())
  , mResolver(parameters.ioContext()) {
}

void TcpBasedProtocol::ClientComponent::onResolved(const ConnectionAttempt::Handler& notify, std::shared_ptr<Socket> socket, const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results) {
  assert(socket != nullptr);

  if (ec || mClosed) {
    socket->close();
    notify(BoostOperationResult<std::shared_ptr<Protocol::Socket>>(ec));
  }
  else {
    assert(!socket->isClosed());
    assert(!socket->isConnected());
    boost::asio::async_connect(socket->basicSocket(), results,
      [](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint& next) { return true; },
      [notify, socket](const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint& endpoint) {
        if (ec) {
          socket->close();
          notify(BoostOperationResult<std::shared_ptr<Protocol::Socket>>(ec));
        }
        else {
          socket->basicSocket().set_option(boost::asio::socket_base::keep_alive(true));
#ifdef __linux__
          int keep_idle = 75; /* seconds */
          setsockopt(socket->basicSocket().native_handle(), SOL_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
#endif
          socket->finishConnecting(notify);
        }
      });
  }
}

std::shared_ptr<Protocol::Socket> TcpBasedProtocol::ClientComponent::openSocket(const ConnectionAttempt::Handler& notify) {
  auto result = this->tcp().createSocket(*this);
  result->setConnectivityStatus(Socket::ConnectivityStatus::connecting);

  auto portString = MakeSharedCopy(std::to_string(mEndPoint.port));
  mResolver.async_resolve(boost::asio::ip::tcp::v4(), mEndPoint.hostname, *portString, [self = SharedFrom(*this), notify, result, portString](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results) {
    self->onResolved(notify, self->tcp().downcastSocket(result), ec, results);
    });

  return result;
}

void TcpBasedProtocol::ClientComponent::close() {
  mResolver.cancel();
  mClosed = true;
}

TcpBasedProtocol::ServerComponent::ServerComponent(const ServerParameters& parameters)
try : Protocol::ServerComponent(parameters)
  , TcpBound(parameters.tcp())
  , mEndPoint(boost::asio::ip::tcp::v4(), parameters.port())
  , mAcceptor(parameters.ioContext()) {
  mAcceptor.open(mEndPoint.protocol());
  mAcceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  mAcceptor.bind(mEndPoint);

  /* TODO: code was cargo culted from previous networking code but seems faulty. See e.g. https://stackoverflow.com/a/40494078, which mentions that
    * > keep-alive on the listening socket is not directly useful
    * > Asio does not copy socket options to the new socket
    * So we'll probably want/need to set the keep_alive(true) option on the socket once a client connects (in the lambda in the "openSocket" method below).
    */
  mAcceptor.set_option(boost::asio::socket_base::keep_alive(true));

  mAcceptor.listen();
}
catch (const boost::system::system_error&) { // Also catch exceptions in member initializers
  std::throw_with_nested(std::runtime_error("Could not set up listener on port " + std::to_string(parameters.port())));
}

uint16_t TcpBasedProtocol::ServerComponent::port() const {
  return mAcceptor.local_endpoint().port();
}

std::shared_ptr<Protocol::Socket> TcpBasedProtocol::ServerComponent::openSocket(const ConnectionAttempt::Handler& notify) {
  auto result = this->tcp().createSocket(*this);
  result->setConnectivityStatus(Socket::ConnectivityStatus::connecting);

  mAcceptor.async_accept(result->basicSocket(), [notify, result, weak = WeakFrom(*this)](const boost::system::error_code& ec) {
    auto self = weak.lock();

    if (ec || self == nullptr) {
      result->close();
      // Don't notify listener of connectivity failure for the never-established connection for the next incoming client
      if (ec != boost::asio::error::make_error_code(boost::asio::error::operation_aborted)) {
        notify(BoostOperationResult<std::shared_ptr<Protocol::Socket>>(ec));
      }
    }
    else {
      result->finishConnecting(notify);
    }
    });

  return result;
}

void TcpBasedProtocol::ServerComponent::close() {
  mAcceptor.cancel();
}


}
