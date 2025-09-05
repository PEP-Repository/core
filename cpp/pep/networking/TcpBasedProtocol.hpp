#pragma once

#include <pep/networking/EndPoint.hpp>
#include <pep/networking/Protocol.hpp>
#include <pep/networking/SocketReadBuffer.hpp>
#include <pep/utils/Shared.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace pep::networking {

/*!
* \brief Base class for networking protocols based on (Boost's implementation of) TCP. Inherit through the TcpBasedProtocolImplementor<> helper (defined below).
*/
class TcpBasedProtocol : public Protocol {
public:
  /// @brief Common ancestor for all nested types, binding them to a TcpBasedProtocol instance (allowing type safe downcasting)
  class TcpBound;

  /// \copydoc Protocol::Socket
  class Socket;

  /// \copydoc Protocol::ClientParameters
  class ClientParameters;
  /// \copydoc Protocol::ServerParameters
  class ServerParameters;

  /// \copydoc Protocol::ClientComponent
  class ClientComponent;
  /// \copydoc Protocol::ServerComponent
  class ServerComponent;


private:
  std::shared_ptr<Socket> downcastSocket(std::shared_ptr<Protocol::Socket> socket) const { return SharedFrom(socket->downcastFor(*this)); }

protected:
  virtual std::shared_ptr<Socket> createSocket(ClientComponent& component) const = 0;
  virtual std::shared_ptr<Socket> createSocket(ServerComponent& component) const = 0;
};


/* \brief Base class for (all) of the TcpBasedProtocol type's nested classes, allowing type safe downcasting.
 * \remark See the InstanceBound<> class documentation for rationale.
 */
class TcpBasedProtocol::TcpBound : private InstanceBound<TcpBasedProtocol> {
protected:
  using InstanceBound<TcpBasedProtocol>::downcastIfBoundTo;

  explicit TcpBound(const TcpBasedProtocol& protocol) noexcept : InstanceBound<TcpBasedProtocol>(protocol) {}

public:
  /* \brief Produces the TcpBasedProtocol (instance/type) to which this object is bound.
   * \return (A reference to) a TcpBasedProtocol instance.
   */
  const TcpBasedProtocol& tcp() const noexcept { return this->boundInstance(); }
};


/*!
 * \brief Wrapper for a networking socket. Abstracts over protocol details (TCP, TLS, ...).
 * \remark (Derived class) instances must be created using std::make_shared (or equivalent) so that instances of this
 *         class can keep themselves alive to perform asynchronous cleanup after their "close()" method has been called.
 */
class TcpBasedProtocol::Socket : public Protocol::Socket, public TcpBound {
  friend class ClientComponent;
  friend class ServerComponent;

private:
  size_t mPendingReadBytes = 0U;
  size_t mPendingWriteBytes = 0U;
  std::shared_ptr<SocketReadBuffer> mReadBuffer;

  [[noreturn]] void raiseTransferStartFailure(size_t& pending, size_t pend, const std::string& reason) const;
  void startTransfer(size_t& pending, size_t pend);
  void onTransferComplete(size_t& pendingBytes, const boost::system::error_code& error, size_t transferred);

protected:
  Socket(const TcpBasedProtocol& protocol, boost::asio::io_context& ioContext);

  using BasicSocket = boost::asio::basic_socket<boost::asio::ip::tcp>;
  virtual BasicSocket& basicSocket() = 0;
  virtual const BasicSocket& basicSocket() const = 0;
  virtual StreamSocket& streamSocket() = 0;

  virtual void finishConnecting(const ConnectionAttempt::Handler& notify);

public:
  using ConnectionAttempt = Protocol::ConnectionAttempt;

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


/// \copydoc Protocol::ClientParameters
class TcpBasedProtocol::ClientParameters : public Protocol::ClientParameters, public TcpBound {
private:
  EndPoint mEndPoint;

protected:
  ClientParameters(const TcpBasedProtocol& protocol, boost::asio::io_context& ioContext, EndPoint endPoint)
    : Protocol::ClientParameters(protocol, ioContext), TcpBound(protocol), mEndPoint(std::move(endPoint)) {
  }

  std::string addressSummary() const override { return mEndPoint.describe() + ':' + std::to_string(mEndPoint.port); }

public:
  /* \brief Produces the endpoint to which the client connects.
   * \return (A reference to) this client's EndPoint.
   */
  const EndPoint& endPoint() const noexcept { return mEndPoint; }
};


/// \copydoc Protocol::ServerParameters
class TcpBasedProtocol::ServerParameters : public Protocol::ServerParameters, public TcpBound {
private:
  uint16_t mPort;

protected:
  ServerParameters(const TcpBasedProtocol& protocol, boost::asio::io_context& ioContext, uint16_t port) noexcept
    : Protocol::ServerParameters(protocol, ioContext), TcpBound(protocol), mPort(port) {
  }

  std::string addressSummary() const override { return "localhost:" + std::to_string(mPort); }

public:
  /* \brief When passed to the constructor, specifies that the server will expose itself on a random port.
   */
  static constexpr uint16_t RANDOM_PORT = 0;

  /* \brief Produces the port on which the server will be exposed.
   * \return The port number for the server.
   * \remark May produce a sentinel value such as RANDOM_PORT. Invoke ServerComponent::port to determine the actual
   *         (non-sentinel) port number on which a server has been exposed.
   */
  uint16_t port() const noexcept { return mPort; }
};


/// \copydoc Protocol::ClientComponent
class TcpBasedProtocol::ClientComponent : public Protocol::ClientComponent, public TcpBound {
private:
  EndPoint mEndPoint;
  boost::asio::ip::tcp::resolver mResolver;
  bool mClosed = false;

  void onResolved(const ConnectionAttempt::Handler& notify, std::shared_ptr<Socket> socket, const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);

public:
  /* \brief Constructor.
   * \param parameters Parameters for this client component.
   */
  explicit ClientComponent(const ClientParameters& parameters);

  /// \copydoc TcpBasedProtocol::ClientParameters::endPoint
  const EndPoint& endPoint() const noexcept { return mEndPoint; }

  /// \copydoc Protocol::ClientComponent::openSocket
  std::shared_ptr<Protocol::Socket> openSocket(const ConnectionAttempt::Handler& notify) override;

  /// \copydoc Protocol::ClientComponent::close
  void close() override;
};


/// \copydoc Protocol::ServerComponent
class TcpBasedProtocol::ServerComponent : public Protocol::ServerComponent, public TcpBound {
private:
  boost::asio::ip::tcp::endpoint mEndPoint;
  boost::asio::ip::tcp::acceptor mAcceptor;

public:
  /* \brief Constructor.
   * \param parameters Parameters for this server component.
   */
  explicit ServerComponent(const ServerParameters& parameters);

  /* \brief Produces the port on which the server is exposed.
   * \return The port number for the server.
   */
  uint16_t port() const;

  /// \copydoc Protocol::ServerComponent::openSocket
  std::shared_ptr<Protocol::Socket> openSocket(const ConnectionAttempt::Handler& notify) override;

  /// \copydoc Protocol::ServerComponent::close
  void close() override;
};


/*!
 * \brief Helper class for TcpBasedProtocol: implementors should inherit from this one instead of directly from TcpBasedProtocol.
 */
template <typename TDerived>
class TcpBasedProtocolImplementor : public ProtocolImplementor<TDerived, TcpBasedProtocol> {
public:
  /// \copydoc TcpBasedProtocol::ClientParameters
  class ClientParameters;
  /// \copydoc TcpBasedProtocol::ServerParameters
  class ServerParameters;
};


/// \copydoc TcpBasedProtocol::ClientParameters
template <typename TDerived>
class TcpBasedProtocolImplementor<TDerived>::ClientParameters : public TcpBasedProtocol::ClientParameters {
public:
  /* \brief Constructor.
   * \param ioContext The I/O context associated with this instance.
   * \param endPoint The endpoint to which the client will connect.
   */
  ClientParameters(boost::asio::io_context& ioContext, EndPoint endPoint)
    : TcpBasedProtocol::ClientParameters(TDerived::Instance(), ioContext, std::move(endPoint)) {
  }
};


/// \copydoc TcpBasedProtocol::ServerParameters
template <typename TDerived>
class TcpBasedProtocolImplementor<TDerived>::ServerParameters : public TcpBasedProtocol::ServerParameters {
public:
  /* \brief Constructor.
   * \param ioContext The I/O context associated with this instance.
   * \param port The port on which the server will be exposed. May be a sentinel value such as TcpBasedProtocol::ServerParameters::RANDOM_PORT.
   */
  ServerParameters(boost::asio::io_context& ioContext, uint16_t port) noexcept
    : TcpBasedProtocol::ServerParameters(TDerived::Instance(), ioContext, port) {
  }
};

}
