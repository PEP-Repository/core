#pragma once

#include <pep/server/Server.hpp>
#include <pep/authserver/OAuthProvider.hpp>
#include <pep/authserver/AuthserverBackend.hpp>


namespace pep {

class Authserver : public SigningServer {
public:
  class Parameters : public SigningServer::Parameters {
  public:
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config);

    const AuthserverBackend::Parameters& getBackendParams() const;

    const OAuthProvider::Parameters& getOAuthParams() const;

  protected:
    void check() override;

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
  std::optional<std::filesystem::path> getStoragePath() override;

private:
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleAsaQuery(std::shared_ptr<SignedAsaQuery> signedRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleAsaMutationRequest(std::shared_ptr<SignedAsaMutationRequest> signedRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleAsaTokenRequest(std::shared_ptr<SignedAsaTokenRequest> signedRequest);

private:
  std::shared_ptr<AuthserverBackend> backend;
  std::shared_ptr<OAuthProvider> oAuth;
};

}
