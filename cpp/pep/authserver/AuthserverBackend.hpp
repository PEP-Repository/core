#pragma once

#include <pep/utils/Configuration_fwd.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/authserver/AuthserverMessages.hpp>
#include <pep/messaging/ServerConnection.hpp>

namespace pep {

class AuthserverBackend {
public:
  class Parameters {
  public:
    Parameters(const Configuration& config);

    std::shared_ptr<messaging::ServerConnection> getAccessManager() const;
    void setAccessManager(std::shared_ptr<messaging::ServerConnection> accessManager);

    std::shared_ptr<const X509Identity> getSigningIdentity() const;
    void setSigningIdentity(std::shared_ptr<const X509Identity> identity);

    std::chrono::seconds getTokenExpiration() const;

    const std::string& getOAuthTokenSecret() const;

    const std::optional<std::filesystem::path>& getStorageFile() const;

    void check() const;

  private:
    std::shared_ptr<messaging::ServerConnection> accessManager;
    std::shared_ptr<const X509Identity> signingIdentity;
    std::chrono::seconds tokenExpiration = std::chrono::seconds::zero();
    std::string oauthTokenSecret;
    std::optional<std::filesystem::path> storageFile;
  };

public:
  explicit AuthserverBackend(const Parameters &params);

  std::vector<std::string> getChecksumChainNames() const;
  rxcpp::observable<ChecksumChainResponse> handleChecksumChainRequest(ChecksumChainRequest request);

  rxcpp::observable<std::optional<std::vector<UserGroup>>>
  findUserGroupsAndStorePrimaryIdIfMissing(
      const std::string &primaryId,
      const std::vector<std::string> &alternativeIds);

  /**
   * \brief Generate an OAuth Token
   * \param uid The uid of the user to generate a token for
   * \param group The user group to generate a token for. It is possible to generate tokens for users/groups unknown to the authserver.
   * \param expirationTime The time at which the token will expire
   * \return The generated OAuthToken
   */
  OAuthToken getToken(const std::string& uid, const std::string& group, const Timestamp& expirationTime) const;
  OAuthToken getToken(const std::string& uid,
                      const UserGroup& group,
                      const std::optional<std::chrono::seconds>& longLivedValidity) const;

  TokenResponse executeTokenRequest(const std::string& accessGroup, const TokenRequest& request);


private:
  void migrateDatabase(const std::filesystem::path& storageFile);

  std::shared_ptr<messaging::ServerConnection> mAccessManager;
  std::shared_ptr<const X509Identity> mSigningIdentity;
  std::chrono::seconds mTokenExpiration;
  std::string mOauthTokenSecret;
};

}
