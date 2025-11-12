#pragma once

#include <pep/auth/ServerTraits.hpp>
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
  virtual ServerTraits serverTraits() const noexcept = 0;

  void check() const override {
    auto traits = this->serverTraits();
    if (!traits.matchesCertificateChain(identity_->getCertificateChain())) {
      throw std::runtime_error("Invalid certificate chain for " + traits.description());
    }

    Server::Parameters::check();
  }

  /// \copydoc Server::Parameters::Parameters
  Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

public:
  std::shared_ptr<const X509Identity> getSigningIdentity() const { return identity_; }
};

}
