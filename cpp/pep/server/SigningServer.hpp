#pragma once

#include <pep/server/Server.hpp>

namespace pep {

/*!
* \brief A Server that can cryptographically sign its messages.
*/
class SigningServer : public Server {
public:
  /// \copydoc Server::Parameters
  class Parameters;

private:
  AsymmetricKey mPrivateKey;
  X509CertificateChain mCertificateChain;

protected:
  explicit SigningServer(std::shared_ptr<Parameters> parameters);

  std::string makeSerializedPingResponse(const PingRequest& request) const override;
  const AsymmetricKey& getPrivateKey() const { return mPrivateKey; }
  const X509CertificateChain& getCertificateChain() const { return mCertificateChain; }
};


/// \copydoc Server::Parameters
class SigningServer::Parameters : public Server::Parameters {
private:
  X509IdentityFilesConfiguration signingIdentity_;

protected:
  void check() const override {
    auto& certificate = *getCertificateChain().cbegin();
    if (!certificate.isPEPServerCertificate()) {
      throw std::runtime_error("certificateChain's leaf certificate must be a PEP server certificate");
    }
    if (certificate.hasTLSServerEKU()) {
      throw std::runtime_error("certificateChain's leaf certificate must not be a TLS certificate");
    }
    Server::Parameters::check();
  }

public:
  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
    : Server::Parameters(io_context, config), signingIdentity_(config, "PEP") {}

  /*!
  * \brief Gets the private key with which the server signs its messages.
  * \return (A reference to) the private key with which the server signs its messages.
  */
  const AsymmetricKey& getPrivateKey() const { return signingIdentity_.getPrivateKey(); }

  /*!
  * \brief Gets the certificate chain that validates the server's private key.
  * \return (A reference to) the certificate chain that validates the server's private key.
  */
  const X509CertificateChain& getCertificateChain() const { return signingIdentity_.getCertificateChain(); }
};

}
