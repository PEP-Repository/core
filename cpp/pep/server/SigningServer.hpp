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
  virtual EnrolledParty enrollsAs() const noexcept = 0;

  void check() const override {
    auto certFor = GetEnrolledParty(*identity_->getCertificateChain().cbegin());
    if (!certFor.has_value()) {
      throw std::runtime_error("certificateChain's leaf certificate must be a PEP server enrollment certificate");
    }
    auto required = this->enrollsAs();
    if (certFor != required) {
      throw std::runtime_error("Server enrolled as " + std::to_string(ToUnderlying(required)) + " cannot be enrolled with certificate chain for " + std::to_string(ToUnderlying(*certFor)));
    }

    Server::Parameters::check();
  }

  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

public:
  std::shared_ptr<const X509Identity> getSigningIdentity() const { return identity_; }
};

}
