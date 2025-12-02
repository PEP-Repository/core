#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/Server.hpp>
#include <pep/server/CertificateRenewalMessages.hpp>

namespace pep {

/*!
* \brief A Server that can cryptographically sign its messages.
*/
class SigningServer : public Server, protected MessageSigner {
public:
  /// \copydoc Server::Parameters
  class Parameters;

  messaging::MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);

  messaging::MessageBatches handleCsrRequest(std::shared_ptr<SignedCsrRequest> signedRequest);
  messaging::MessageBatches handleCertificateReplacementRequest(std::shared_ptr<SignedCertificateReplacementRequest> signedRequest);
  messaging::MessageBatches handleCertificateReplacementCommitRequest(std::shared_ptr<SignedCertificateReplacementCommitRequest> signedRequest);

protected:
  explicit SigningServer(std::shared_ptr<Parameters> parameters);

private:
  std::shared_ptr<X509IdentityFilesConfiguration> mIdentityFilesConfig;
  std::optional<AsymmetricKey> mNewPrivateKey;
};


/// \copydoc Server::Parameters
class SigningServer::Parameters : public Server::Parameters {
private:
  std::shared_ptr<X509IdentityFilesConfiguration> mIdentityFilesConfig;

protected:
  void check() const override {
    auto traits = this->serverTraits();
    if (!traits.signingIdentityMatches(mIdentityFilesConfig->identity()->getCertificateChain())) {
      throw std::runtime_error("Invalid certificate chain for " + traits.description());
    }

    Server::Parameters::check();
  }

  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

public:
  std::shared_ptr<const X509Identity> getSigningIdentity() const { return mIdentityFilesConfig->identity(); }
  std::shared_ptr<X509IdentityFilesConfiguration> getIdentityFilesConfig() const { return mIdentityFilesConfig; }
};

}
