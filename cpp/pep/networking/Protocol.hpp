#pragma once

#include <pep/networking/ConnectivityAttempt.hpp>
#include <pep/networking/InstanceBound.hpp>
#include <pep/networking/Transport.hpp>
#include <pep/utils/Singleton.hpp>
#include <pep/async/IoContext_fwd.hpp>

namespace pep::networking {

/* \brief (Abstract) base class for specific network protocol types. Inherit through the ProtocolImplementor<> helper (defined below).
 * \remark If/since your implementation requires specific state for sockets and/or clients and/or servers, inherit the nested classes
 *         as well, naming the nested derived classes the same as their base class. E.g. you'd create
 *         class MyProtocol: public Protocol {
 *         public:
 *           class ClientParameters : public Protocol::ClientParameters {
 *             // ...implementation...
 *           }
 *         
 *           class ClientComponent : public Protocol::ClientComponent {
 *           public:
 *             explicit ClientComponent(const ClientParameters& parameters) { // receives _derived_ type parameter
 *               // ...implementation...
 *             }
 *           };
 *         };
 */
class Protocol : boost::noncopyable {
public:
  /// @brief Common ancestor for all nested types, binding them to a Protocol instance (allowing type safe downcasting) and an I/O context
  class Bound;

  /// @brief (Base class for) protocol specific objects providing communications with a connected party
  class Socket;
  /// @brief Typedef for socket connection attempts
  using ConnectionAttempt = ConnectivityAttempt<Socket>;

  /// @brief (Base class for) protocol specific state needed by a Client
  class ClientComponent;
  /// @brief (Base class for) protocol specific state needed by a Server
  class ServerComponent;
  /// @brief (Abstract) common ancestor for ClientComponent and ServerComponent
  class NodeComponent;

  /// @brief (Base class for) protocol specific parameters needed to create a ClientComponent
  class ClientParameters;
  /// @brief (Base class for) protocol specific parameters needed to create a ServerComponent
  class ServerParameters;
  /// @brief (Abstract) common ancestor for ClientParameters and ServerParameters
  class NodeParameters;

  virtual ~Protocol() noexcept = default;
  /// @brief Produces the name of the protocol
  virtual std::string name() const = 0;

protected:
  virtual std::shared_ptr<ClientComponent> createComponent(const ClientParameters& parameters) const = 0; // Produces a shared_ptr (instead of a unique_ptr) so that ClientComponent (inheritors) may use shared_from_this
  virtual std::shared_ptr<ServerComponent> createComponent(const ServerParameters& parameters) const = 0; // Produces a shared_ptr (instead of a unique_ptr) so that ServerComponent (inheritors) may use shared_from_this
  virtual std::shared_ptr<ClientParameters> createClientParameters(const ServerComponent& server) const = 0;
};


/* \brief Base class for (all) of the Protocol type's nested classes, allowing type safe downcasting.
 * \remark See the InstanceBound<> class documentation for rationale.
 */
class Protocol::Bound : private InstanceBound<Protocol> { // Note private inheritance: use the "Protocol::Bound::protocol" method instead of "InstanceBound<>::boundInstance"

  friend class InstanceBound<Protocol>; // Allow base class to static_cast<> to derived despite private inheritance

private:
  boost::asio::io_context& mIoContext;

protected:
  explicit Bound(const Protocol& protocol, boost::asio::io_context& ioContext) noexcept : InstanceBound<Protocol>(protocol), mIoContext(ioContext) {}

  using InstanceBound<Protocol>::downcastIfBoundTo; // Provide access to privately inherited method

public:
  /* \brief Produces the Protocol (instance/type) to which this object is bound.
   * \return (A reference to) a Protocol instance.
   */
  const Protocol& protocol() const noexcept { return this->boundInstance(); }

  /* \brief Produces the I/O context associated with this object.
   * \return (A reference to) a Boost io_context.
   */
  boost::asio::io_context& ioContext() const noexcept { return mIoContext; }
};

/*!
 * \brief Wrapper for a networking socket. Abstracts over protocol details (TCP, TLS, ...).
 * \remark (Derived class) instances must be created using std::make_shared (or equivalent) so that instances of this
 *         class can keep themselves alive to perform asynchronous cleanup after their "close()" method has been called.
 */
class Protocol::Socket : public Transport, public Bound, public std::enable_shared_from_this<Socket> {
protected:
  explicit Socket(const Protocol& protocol, boost::asio::io_context& ioContext) : Bound(protocol, ioContext) {}

public:
  template <typename TProtocol>
  typename TProtocol::Socket& downcastFor(const TProtocol& protocol) {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::Socket>(protocol);
  }
};


/*!
 * \brief Base class for ClientParameters and ServerParameters.
 */
class Protocol::NodeParameters : public Bound {
protected:
  explicit NodeParameters(const Protocol& protocol, boost::asio::io_context& ioContext) noexcept : Bound(protocol, ioContext) {}
  virtual std::string addressSummary() const = 0;

public:
  /* \brief Produces a (human-readable) string representation of the address associated with the node.
   * \return A string representing the address of (the server associated with) the node.
   */
  std::string address() const { return this->protocol().name() + "://" + this->addressSummary(); }
};


/*!
 * \brief Base class for protocol specific client parameters.
 */
class Protocol::ClientParameters : public NodeParameters {
public:
  /*!
   * \brief Constructor.
   * \param protocol The Protocol instance with which this object is associated.
   * \param ioContext The I/O context associated with this object.
   */
  explicit ClientParameters(const Protocol& protocol, boost::asio::io_context& ioContext) noexcept
    : NodeParameters(protocol, ioContext) {
  }

  /*!
   * \brief Downcasts this instance to client parameters for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  const typename TProtocol::ClientParameters& downcastFor(const TProtocol& protocol) const {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ClientParameters>(protocol);
  }

  /*!
   * \brief Creates a protocol specific client component according to the parameters that this object represents.
   * \return A ClientComponent usable by a Client class to perform protocol specific actions.
   */
  std::shared_ptr<Protocol::ClientComponent> createComponent() const { return this->protocol().createComponent(*this); }
};


/*!
 * \brief Base class for protocol specific server parameters.
 */
class Protocol::ServerParameters : public NodeParameters {
public:
  /*!
   * \brief Constructor.
   * \param protocol The Protocol instance with which this object is associated.
   * \param ioContext The I/O context associated with this object.
   */
  explicit ServerParameters(const Protocol& protocol, boost::asio::io_context& ioContext) noexcept
    : NodeParameters(protocol, ioContext) {
  }

  /*!
   * \brief Downcasts this instance to server parameters for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  const typename TProtocol::ServerParameters& downcastFor(const TProtocol& protocol) const {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ServerParameters>(protocol);
  }

  /*!
   * \brief Creates a protocol specific server component according to the parameters that this object represents.
   * \return A ServerComponent usable by a Server class to perform protocol specific actions.
   */
  std::shared_ptr<Protocol::ServerComponent> createComponent() const { return this->protocol().createComponent(*this); }
};


/*!
 * \brief Base class for protocol specific node components.
 */
class Protocol::NodeComponent : public std::enable_shared_from_this<NodeComponent>, public Bound, boost::noncopyable {
protected:
  explicit NodeComponent(const NodeParameters& parameters) noexcept
    : Bound(parameters.protocol(), parameters.ioContext()), mConnectionAddress(parameters.address()) {
  }

  const std::string& connectionAddress() const { return mConnectionAddress; }

public:
  /*!
   * \brief Produces a (human-readable) description of the node (component).
   * \return A string describing the node (component).
   */
  virtual std::string describe() const = 0;

  /*!
   * \brief Releases the node component's resources.
   */
  virtual void close() = 0;

  /*!
   * \brief Opens a socket, returning it immediately so that it can be (memory) managed by the Node.
   * \param notify A function object that will be invoked when socket connectivity has been established or failed.
   * \return The created socket.
   * \remark The returned socket may not (yet) be fully opened when returned: see the "notify" parameter.
   */
  virtual std::shared_ptr<Protocol::Socket> openSocket(const ConnectionAttempt::Handler& notify) = 0;

private:
  std::string mConnectionAddress;
};


/*!
 * \brief Base class for protocol specific client components.
 */
class Protocol::ClientComponent : public NodeComponent {
public:
  /*!
   * \brief Constructor.
   * \param parameters Parameters used to initialize the client component.
   */
  explicit ClientComponent(const ClientParameters& parameters)
    : NodeComponent(parameters) {
  }

  /*!
   * \brief Produces a (human-readable) description of the client.
   * \return A string describing the client.
   */
  std::string describe() const override { return "client to " + this->connectionAddress(); }

  /*!
   * \brief Downcasts this instance to a client component for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \return This object, downcast to a client component for the specified protocol (type).
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  typename TProtocol::ClientComponent& downcastFor(const TProtocol& protocol) {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ClientComponent>(protocol);
  }

  /*!
   * \brief Downcasts this instance to a client component for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \return This object, downcast to a client component for the specified protocol (type).
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  const typename TProtocol::ClientComponent& downcastFor(const TProtocol& protocol) const {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ClientComponent>(protocol);
  }
};


/*!
 * \brief Base class for protocol specific server components.
 */
class Protocol::ServerComponent : public NodeComponent {
public:
  /*!
   * \brief Constructor.
   * \param parameters Parameters used to initialize the server component.
   */
  explicit ServerComponent(const ServerParameters& parameters)
    : NodeComponent(parameters) {
  }

  /*!
   * \brief Produces a (human-readable) description of the server.
   * \return A string describing the server.
   */
  std::string describe() const override { return "server at " + this->connectionAddress(); }

  /*!
   * \brief Downcasts this instance to a server component for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \return This object, downcast to a server component for the specified protocol (type).
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  typename TProtocol::ServerComponent& downcastFor(const TProtocol& protocol) {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ServerComponent>(protocol);
  }

  /*!
   * \brief Downcasts this instance to a server component for the specified protocol type.
   * \param protocol The protocol instance with which this object must be associated.
   * \return This object, downcast to a server component for the specified protocol (type).
   * \remark See the InstanceBound<> class documentation for rationale.
   */
  template <typename TProtocol>
  const typename TProtocol::ServerComponent& downcastFor(const TProtocol& protocol) const {
    static_assert(std::is_base_of<Protocol, TProtocol>::value, "Template parameter must inherit from Protocol");
    return this->downcastIfBoundTo<typename TProtocol::ServerComponent>(protocol);
  }

  /*!
   * \brief Produces client parameters allowing (local) clients to connect to this server.
   * \return Client parameters to connect to this server.
   */
  std::shared_ptr<ClientParameters> createClientParameters() const { return this->protocol().createClientParameters(*this); }
};

/*!
 * \brief Helper class for Protocol: implementors should inherit from this one instead of directly from Protocol.
 */
template <typename TDerived, typename TBase = Protocol>
class ProtocolImplementor : public StaticSingleton<TDerived>, public TBase {
  static_assert(std::is_base_of<Protocol, TBase>::value, "Template parameter 'TBase' must (be or) inherit from Protocol");

protected:
  std::shared_ptr<Protocol::ClientComponent> createComponent(const Protocol::ClientParameters& parameters) const override {
    return std::make_shared<typename TDerived::ClientComponent>(parameters.downcastFor(static_cast<const TDerived&>(*this)));
  }

  std::shared_ptr<Protocol::ServerComponent> createComponent(const Protocol::ServerParameters& parameters) const override {
    return std::make_shared<typename TDerived::ServerComponent>(parameters.downcastFor(static_cast<const TDerived&>(*this)));
  }

public:
  using StaticSingleton<TDerived>::Instance;
};

}
