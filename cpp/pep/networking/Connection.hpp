#pragma once

#include <pep/networking/Protocol.hpp>

namespace pep::networking {

/*!
 * \brief A Transport that's self-finalizing.
 */
class Connection : public Transport {
private:
  std::shared_ptr<Protocol::Socket> mSocket = nullptr;
  EventSubscription mSocketConnectivityForwarding;

  template <typename TResult>
  std::shared_ptr<Protocol::Socket> getSocketOrNotifyTransferFailure(const std::function<void(const TResult&)>& onTransferred) {
    auto result = mSocket;
    if (result == nullptr) {
      onTransferred(TResult::Failure(std::make_exception_ptr(std::runtime_error("Can't transfer over a disconnected socket"))));
    }
    return result;
  }

protected:
  Connection() noexcept = default;

  using SocketConnectivityChangeHandler = decltype(Protocol::Socket::onConnectivityChange)::Handler;
  void setSocket(std::shared_ptr<Protocol::Socket> socket, SocketConnectivityChangeHandler handleSocketConnectivityChange);
  void discardSocket();

public:
  using Attempt = ConnectivityAttempt<Connection>;

  ~Connection() noexcept override { Connection::close(); } // Explicitly scoped to avoid dynamic dispatch

  /// \copydoc Transport::close
  void close() override;

  /// \copydoc Transport::remoteAddress
  std::string remoteAddress() const override;

  /// \copydoc Transport::asyncRead
  void asyncRead(void* destination, size_t bytes, const SizedTransfer::Handler& onTransferred) override;

  /// \copydoc Transport::asyncReadUntil
  void asyncReadUntil(const char* delimiter, const DelimitedTransfer::Handler& onTransferred) override;

  /// \copydoc Transport::asyncReadAll
  void asyncReadAll(const DelimitedTransfer::Handler& onTransferred) override;

  /// \copydoc Transport::asyncWrite
  void asyncWrite(const void* source, size_t bytes, const SizedTransfer::Handler& onTransferred) override;
};

}
