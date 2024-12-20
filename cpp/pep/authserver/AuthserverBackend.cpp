#include <pep/authserver/AuthserverBackend.hpp>

#include <pep/auth/UserGroups.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <rxcpp/operators/rx-subscribe_on.hpp>
#include <rxcpp/operators/rx-merge.hpp>
#include <rxcpp/operators/rx-concat.hpp>

#include <boost/algorithm/hex.hpp>

namespace pep {

static const std::string LOG_TAG ("AuthserverBackend");

AuthserverBackend::Parameters::Parameters(const Configuration& config) {
  std::filesystem::path oauthTokenSecretFile;
  std::filesystem::path storageFile;
  try {
    tokenExpiration = std::chrono::seconds(config.get<unsigned int>("TokenExpirationSeconds"));
    oauthTokenSecretFile = std::filesystem::canonical(config.get<std::filesystem::path>("OAuthTokenSecretFile"));

    storageFile = config.get<std::filesystem::path>("StorageFile");
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    Configuration secretFile = Configuration::FromFile(oauthTokenSecretFile);

    std::string secretHex = secretFile.get<std::string>("OAuthTokenSecret");
    oauthTokenSecret = boost::algorithm::unhex(secretHex);
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with oauth file: " << e.what();
    throw;
  }

  storage = std::make_shared<AuthserverStorage>(storageFile);
}

std::shared_ptr<AuthserverStorage> AuthserverBackend::Parameters::getStorage() const {
  return storage;
}

std::chrono::seconds AuthserverBackend::Parameters::getTokenExpiration() const {
  return tokenExpiration;
}

const std::string& AuthserverBackend::Parameters::getOAuthTokenSecret() const {
  return oauthTokenSecret;
}

void AuthserverBackend::Parameters::check() {
  if(!storage) {
    throw std::runtime_error("storage must be set");
  }
  if(tokenExpiration.count() == 0) {
    throw std::runtime_error("tokenExpiration must be set");
  }
  if(oauthTokenSecret.empty()) {
    throw std::runtime_error("oauthTokenSecret must be set");
  }
}

std::vector<std::string> AuthserverBackend::getChecksumChainNames() const {
  return storage->getChecksumChainNames();
}

void AuthserverBackend::computeChecksumChainChecksum(
    const std::string& chain,
    std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum,
    uint64_t& checkpoint) {
  storage->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

std::optional<int64_t> AuthserverBackend::findUserAndStorePrimaryIdIfMissing(
    const std::string& primaryId, const std::vector<std::string>& alternativeIds) {
  std::optional<int64_t> userId = storage->findInternalId(primaryId);
  if(!userId) {
    userId = storage->findInternalId(alternativeIds);
    if(userId) {
      storage->addIdentifierForUser(*userId, primaryId);
    }
  }
  return userId;
}

std::unordered_set<std::string> AuthserverBackend::getUserGroups(int64_t internalId) const {
  return storage->getUserGroups(internalId);
}

OAuthToken AuthserverBackend::getToken(const std::string& uid, const std::string& group, const Timestamp& expirationTime) const {
  auto now = std::chrono::system_clock::now();
  return OAuthToken::Generate(
    oauthTokenSecret, uid, group,
    static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now)),
    static_cast<uint64_t>(expirationTime.getTime())
  );
}

OAuthToken AuthserverBackend::getToken(int64_t internalId, const std::string& uid, const std::string& group, const std::optional<std::chrono::seconds>& longLivedValidity) const {
  if(!storage->userInGroup(internalId, group)) {
    throw Error(std::string("User ") + uid + " is not a member of group " + group);
  }
  auto maxValidity = storage->getMaxAuthValidity(group);

  std::chrono::seconds validity;
  if (longLivedValidity) {
    if (!maxValidity) {
      throw Error("A long-lived token was requested but this user is not allowed to request long-lived tokens.");
    }
    if(*longLivedValidity > *maxValidity) {
      throw Error(std::string("A token was requested for ") + chrono::ToString(*longLivedValidity) +
          " but this user can only request tokens for a maximum of " + chrono::ToString(*maxValidity) + " for this group");
    }
    validity = *longLivedValidity;
  }
  else {
    validity = this->tokenExpiration;
  }
  auto now = std::chrono::system_clock::now();
  return OAuthToken::Generate(
    oauthTokenSecret, uid, group,
    static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now)),
    static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now + validity))
  );
}

std::optional<std::chrono::seconds> AuthserverBackend::getMaxAuthValidity(const std::string& group) {
  return storage->getMaxAuthValidity(group);
}

AsaQueryResponse
AuthserverBackend::executeAsaQuery(const std::string& accessGroup, const AsaQuery& query) {
  user_group::EnsureAccess({user_group::AccessAdministrator}, accessGroup, "AsaQuery");

  return storage->executeQuery(query);

}

AsaMutationResponse
AuthserverBackend::executeAsaMutationRequest(const std::string& accessGroup, const AsaMutationRequest& request) {
  // Check access
  user_group::EnsureAccess({user_group::AccessAdministrator}, accessGroup);

  // Execute mutations
  std::vector<rxcpp::observable<rxcpp::observable<std::string>>> observables;
  for (auto& x : request.mCreateUser) {
    storage->createUser(x.mUid);
    LOG(LOG_TAG, info) << "Created user " << Logging::Escape(x.mUid);
  }
  for (auto& x : request.mRemoveUser) {
    storage->removeUser(x.mUid);
    LOG(LOG_TAG, info) << "Removed user " << Logging::Escape(x.mUid);
  }
  for (auto& x : request.mAddUserIdentifier) {
    storage->addIdentifierForUser(x.mExistingUid, x.mNewUid);
    LOG(LOG_TAG, info) << "Added identifier " << Logging::Escape(x.mNewUid) << " for user " << Logging::Escape(x.mExistingUid);
  }
  for (auto& x : request.mRemoveUserIdentifier) {
    storage->removeIdentifierForUser(x.mUid);
    LOG(LOG_TAG, info) << "Removed identifier " << Logging::Escape(x.mUid);
  }
  for (auto& x : request.mCreateUserGroup) {
    storage->createGroup(x.mName, x.mProperties);
    LOG(LOG_TAG, info) << "Created group " << Logging::Escape(x.mName);
  }
  for (auto& x : request.mRemoveUserGroup) {
    storage->removeGroup(x.mName);
    LOG(LOG_TAG, info) << "Removed group " << Logging::Escape(x.mName);
  }
  for (auto& x : request.mModifyUserGroup) {
    storage->modifyGroup(x.mName, x.mProperties);
    LOG(LOG_TAG, info) << "Modified group " << Logging::Escape(x.mName);
  }
  for (auto& x : request.mAddUserToGroup) {
    storage->addUserToGroup(x.mUid, x.mGroup);
    LOG(LOG_TAG, info) << "Added user to group " << Logging::Escape(x.mGroup);
  }
  for (auto& x : request.mRemoveUserFromGroup) {
    storage->removeUserFromGroup(x.mUid, x.mGroup);
    LOG(LOG_TAG, info) << "Removed user from group " << Logging::Escape(x.mGroup);
  }

  return AsaMutationResponse();
}

AsaTokenResponse
AuthserverBackend::executeAsaTokenRequest(const std::string& accessGroup, const AsaTokenRequest& request) {
  // Check access
  user_group::EnsureAccess({user_group::AccessAdministrator}, accessGroup);

  auto token = getToken(request.mSubject, request.mGroup, request.mExpirationTime);

  return AsaTokenResponse(token.getSerializedForm());
}

std::filesystem::path AuthserverBackend::getStoragePath() {
  return storage->getPath();
}

}
