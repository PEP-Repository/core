#pragma once

#include <pep/server/SigningServer.hpp>
#include <pep/authserver/OAuthProvider.hpp>
#include <pep/authserver/AuthserverBackend.hpp>


namespace pep {

class Authserver : public SigningServer {
public:
  class Parameters : public SigningServer::Parameters {
  public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

    const AuthserverBackend::Parameters& getBackendParams() const;

    const OAuthProvider::Parameters& getOAuthParams() const;

  protected:
    ServerTraits serverTraits() const noexcept override { return ServerTraits::AuthServer(); }
    void check() const override;

  private:
    AuthserverBackend::Parameters backendParams_;
    OAuthProvider::Parameters oauthParams_;
  };

public:
  explicit Authserver(std::shared_ptr<Parameters> parameters);

protected:
  const ChecksumChains& checksumChains() const noexcept override { return checksumChains_; }

private:
  messaging::MessageBatches handleTokenRequest(std::shared_ptr<SignedTokenRequest> signedRequest);

  std::shared_ptr<AuthserverBackend> backend_;
  std::shared_ptr<OAuthProvider> oAuth_;
  ChecksumChains checksumChains_; // none
};

}
