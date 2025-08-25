#pragma once

#include <pep/networking/TcpBasedProtocol.hpp>
#include <pep/crypto/X509Certificate.hpp>

#include <boost/asio/ssl/context.hpp>
#include <filesystem>

namespace pep::networking {

/*!
* \brief TLS-enabled TCP networking.
*/
class Tls : public TcpBasedProtocolImplementor<Tls> {
private:
  class NodeParameters;

public:
  /// \copydoc TcpBasedProtocolImplementor<Tls>::ClientParameters
  class ClientParameters;
  /// \copydoc TcpBasedProtocolImplementor<Tls>::ServerParameters
  class ServerParameters;
  
  /// @brief Protocol specific state needed by a TLS node: (abstract) common ancestor for Tls::ClientComponent and Tls::ServerComponent
  class NodeComponent;

  /// @brief Protocol specific state needed by a TLS client
  class ClientComponent;
  /// @brief Protocol specific state needed by a TLS server
  class ServerComponent;

  /// \copydoc Protocol::name
  std::string name() const override { return "tls"; }

protected:
  std::shared_ptr<TcpBasedProtocol::Socket> createSocket(TcpBasedProtocol::ClientComponent& component) const override;
  std::shared_ptr<TcpBasedProtocol::Socket> createSocket(TcpBasedProtocol::ServerComponent& component) const override;
  std::shared_ptr<Protocol::ClientParameters> createClientParameters(const Protocol::ServerComponent& server) const override;
};


/// @brief Protocol specific state needed by a TLS node: (abstract) common ancestor for Tls::ClientComponent and Tls::ServerComponent
class Tls::NodeComponent : boost::noncopyable {
  friend class Tls;

private:
  boost::asio::ssl::context mSslContext;

protected:
  NodeComponent();

  boost::asio::ssl::context& sslContext() noexcept { return mSslContext; }
  const boost::asio::ssl::context& sslContext() const noexcept { return mSslContext; }
};


/// \copydoc TcpBasedProtocolImplementor<Tls>::ClientParameters
class Tls::ClientParameters : public TcpBasedProtocolImplementor<Tls>::ClientParameters {
private:
  std::optional<std::filesystem::path> mCaCertFilePath;
  bool mSkipPeerVerification = false;

public:
  /* \brief Constructor.
   * \param ioContext The I/O context associated with the client.
   * \param endPoint The endpoint to which the client will connect.
   */
  ClientParameters(boost::asio::io_context& ioContext, EndPoint endPoint);

  /* \brief Gets the path to the file containing the (PEM-encoded) CA certificate (if available).
   * \return (A reference to) this instance's path to the (PEM-encoded) CA certificate, or std::nullopt if no certificate file has been configured.
   */
  const std::optional<std::filesystem::path>& caCertFilePath() const noexcept { return mCaCertFilePath; }

  /* \brief Sets the path to the file containing the (PEM-encoded) CA certificate (if available).
   * \param assign The path to the (PEM-encoded) CA certificate, or std::nullopt to configure the parameters without a CA certificate file path.
   * \return (A reference to) this instance's path to the (PEM-encoded) CA certificate, or std::nullopt if no certificate file has been configured.
   */
  const std::optional<std::filesystem::path>& caCertFilePath(const std::optional<std::filesystem::path>& assign) { return mCaCertFilePath = assign; }

  /* \brief Gets whether the client will skip verification of its peer's certificate.
   * \return TRUE if the client will skip peer certificate verification; FALSE if it will perform said verification.
   */
  bool skipPeerVerification() const noexcept { return mSkipPeerVerification; }

  /* \brief Sets whether the client will skip verification of its peer's certificate.
   * \param assign TRUE to have the client skip peer certificate verification; FALSE to have it perform said verification.
   * \return TRUE if the client will skip peer certificate verification; FALSE if it will perform said verification.
   */
  bool skipPeerVerification(bool assign) noexcept { return mSkipPeerVerification = assign; }
};


/// \copydoc TcpBasedProtocolImplementor<Tls>::ServerParameters
class Tls::ServerParameters : public TcpBasedProtocolImplementor<Tls>::ServerParameters {
private:
  X509IdentityFilesConfiguration mIdentity;
  bool mSkipCertificateSecurityLevelCheck = false;

public:
  /* \brief Constructor.
   * \param ioContext The I/O context associated with the server.
   * \param endPoint The port on which the server will be exposed. May be a sentinel value such as TcpBasedProtocol::ServerParameters::RANDOM_PORT.
   * \param identity Configuration specifying the server's TLS identity.
   */
  ServerParameters(boost::asio::io_context& ioContext, uint16_t port, X509IdentityFilesConfiguration identity);

  /* \brief Gets the configuration containing the server's TLS identity.
   * \return (A reference to) this instance's TLS identity configuration.
   */
  const X509IdentityFilesConfiguration& identity() const noexcept { return mIdentity; }

  /* \brief Gets whether the server will skip the security check of its certificate.
   * \return TRUE if the server will skip its certificate security level check; FALSE if it will perform said check.
   */
  bool skipCertificateSecurityLevelCheck() const noexcept { return mSkipCertificateSecurityLevelCheck; }

  /* \brief Sets whether the server will skip the security check of its certificate.
   * \param assign TRUE to have the server skip its certificate security level check; FALSE to have it perform said check.
   * \return TRUE if the server will skip its certificate security level check; FALSE if it will perform said check.
   */
  bool skipCertificateSecurityLevelCheck(bool assign) noexcept { return mSkipCertificateSecurityLevelCheck = assign; }
};


/// @brief Protocol specific state needed by a TLS client
class Tls::ClientComponent : public TcpBasedProtocolImplementor<Tls>::ClientComponent, public NodeComponent {
public:
  /* \brief Constructor.
   * \param parameters Parameters for this client component.
   */
  explicit ClientComponent(const ClientParameters& parameters);
};


/// @brief Protocol specific state needed by a TLS server
class Tls::ServerComponent : public TcpBasedProtocolImplementor<Tls>::ServerComponent, public NodeComponent {
public:
  /* \brief Constructor.
   * \param parameters Parameters for this server component.
   */
  explicit ServerComponent(const ServerParameters& parameters);
};

}
