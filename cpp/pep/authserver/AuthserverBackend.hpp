#pragma once
#include <pep/utils/Configuration.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/authserver/AuthserverStorage.hpp>


namespace pep {

class AuthserverBackend {
public:
  class Parameters {
  public:
    Parameters(const Configuration& config);

    std::shared_ptr<AuthserverStorage> getStorage() const;

    std::chrono::seconds getTokenExpiration() const;

    const std::string& getOAuthTokenSecret() const;

    void check();

  private:
    std::shared_ptr<AuthserverStorage> storage;
    std::chrono::seconds tokenExpiration = std::chrono::seconds::zero();
    std::string oauthTokenSecret;
  };

public:
  explicit AuthserverBackend(const Parameters& params) :
    storage(params.getStorage()),
    tokenExpiration(params.getTokenExpiration()),
    oauthTokenSecret(params.getOAuthTokenSecret())
    {}

  std::vector<std::string> getChecksumChainNames() const;
  void computeChecksumChainChecksum(const std::string& chain,
                                    std::optional<uint64_t> maxCheckpoint,
                                    uint64_t& checksum,
                                    uint64_t& checkpoint);

  std::optional<int64_t> findUserAndStorePrimaryIdIfMissing(const std::string& PrimaryId,
                                                           const std::vector<std::string>& alternativeIds);

  std::unordered_set<std::string> getUserGroups(int64_t internalId) const;
  /**
   * \brief Generate an OAuth Token
   * \param uid The uid of the user to generate a token for
   * \param group The user group to generate a token for. It is possible to generate tokens for users/groups unknown to the authserver.
   * \param expirationTime The time at which the token will expire
   * \return The generated OAuthToken
   */
  OAuthToken getToken(const std::string& uid, const std::string& group, const Timestamp& expirationTime) const;
  OAuthToken getToken(int64_t internalId,
                      const std::string& uid,
                      const std::string& group,
                      const std::optional<std::chrono::seconds>& longLivedValidity) const;

  std::optional<std::chrono::seconds> getMaxAuthValidity(const std::string& group);

  AsaQueryResponse executeAsaQuery(const std::string& accessGroup, const AsaQuery& query);
  AsaMutationResponse executeAsaMutationRequest(const std::string& accessGroup, const AsaMutationRequest& request);
  AsaTokenResponse executeAsaTokenRequest(const std::string& accessGroup, const AsaTokenRequest& request);

  std::filesystem::path getStoragePath();

private:
  std::shared_ptr<AuthserverStorage> storage;
  std::chrono::seconds tokenExpiration;
  std::string oauthTokenSecret;
};

}
