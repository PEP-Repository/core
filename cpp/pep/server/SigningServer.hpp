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
  void check() const override {
    auto& certificate = *identity_->getCertificateChain().cbegin();
    if (!IsServerEnrollmentCertificate(certificate)) {
      throw std::runtime_error("certificateChain's leaf certificate must be a PEP server enrollment certificate");
    }
    Server::Parameters::check();
  }

  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

public:
  std::shared_ptr<const X509Identity> getSigningIdentity() const { return identity_; }
};

}
