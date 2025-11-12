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
    EnrolledParty enrollsAs() const noexcept override { return EnrolledParty::AuthServer; }
    void check() const override;

  private:
    AuthserverBackend::Parameters backendParams;
    OAuthProvider::Parameters oauthParams;
  };

public:
  explicit Authserver(std::shared_ptr<Parameters> parameters);

protected:
  std::string describe() const override;
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum, uint64_t& checkpoint) override;

private:
  messaging::MessageBatches handleTokenRequest(std::shared_ptr<SignedTokenRequest> signedRequest);
  messaging::MessageBatches handleChecksumChainRequest(std::shared_ptr<SignedChecksumChainRequest> signedRequest);


  std::shared_ptr<AuthserverBackend> mBackend;
  std::shared_ptr<OAuthProvider> mOAuth;
};

}
