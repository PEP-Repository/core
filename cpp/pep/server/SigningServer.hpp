#pragma once

#include <pep/auth/EnrolledParty.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/Server.hpp>

namespace pep {

/*!
* \brief A Server that can cryptographically sign its messages.
*/
class SigningServer : public Server, protected MessageSigner {
public:
  /// \copydoc Server::Parameters
  class Parameters;

  messaging::MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);

protected:
  explicit SigningServer(std::shared_ptr<Parameters> parameters);
};


/// \copydoc Server::Parameters
class SigningServer::Parameters : public Server::Parameters {
private:
  std::shared_ptr<X509Identity> identity_;

protected:
  virtual std::unordered_set<std::string> certificateSubjects() const noexcept = 0;

  void check() const override {
    auto description = *this->certificateSubjects().begin();
    const auto& chain = identity_->getCertificateChain();
    if (chain.empty()) {
      throw std::runtime_error("Invalid certificate chain for " + description + ": empty");
    }
    auto cert = chain.front();
    if (!IsServerSigningCertificate(cert)) {
      throw std::runtime_error("Invalid certificate chain for " + description + ": not a PEP server signing certificate");
    }
    auto ou = cert.getOrganizationalUnit().value_or({});
    if (!this->certificateSubjects().contains(ou)) {
      throw std::runtime_error("Invalid certificate chain for " + description + ": issued to \"" + ou + '"');
    }

    Server::Parameters::check();
  }

  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

public:
  std::shared_ptr<const X509Identity> getSigningIdentity() const { return identity_; }
};

}
