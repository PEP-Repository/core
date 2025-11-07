#include <pep/authserver/AuthserverBackend.hpp>

#include <pep/auth/UserGroup.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>

#include <boost/algorithm/hex.hpp>
#include <fstream>
#include <pep/messaging/MessageSequence.hpp>

#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-take.hpp>

namespace pep {

static const std::string LOG_TAG ("AuthserverBackend");

AuthserverBackend::Parameters::Parameters(const Configuration& config) {
  std::filesystem::path oauthTokenSecretFile;
  try {
    tokenExpiration = std::chrono::seconds(config.get<unsigned int>("TokenExpirationSeconds"));
    oauthTokenSecretFile = std::filesystem::canonical(config.get<std::filesystem::path>("OAuthTokenSecretFile"));

    storageFile = config.get<std::optional<std::filesystem::path>>("StorageFile");
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
}

std::shared_ptr<messaging::ServerConnection>
AuthserverBackend::Parameters::getAccessManager() const {
  return this->accessManager;
}

void AuthserverBackend::Parameters::setAccessManager(
    std::shared_ptr<messaging::ServerConnection> accessManager) {
  this->accessManager = accessManager;
}

std::shared_ptr<const X509Identity> AuthserverBackend::Parameters::getSigningIdentity() const {
  return signingIdentity;
}
void AuthserverBackend::Parameters::setSigningIdentity(std::shared_ptr<const X509Identity> identity) {
  this->signingIdentity = identity;
}

std::chrono::seconds AuthserverBackend::Parameters::getTokenExpiration() const {
  return tokenExpiration;
}

const std::string& AuthserverBackend::Parameters::getOAuthTokenSecret() const { return oauthTokenSecret; }

const std::optional<std::filesystem::path>& AuthserverBackend::Parameters::getStorageFile() const {return storageFile; }

void AuthserverBackend::Parameters::check() const {
  if (!accessManager) {
    throw std::runtime_error("AccessManager must be set");
  }
  if(storageFile && storageFile->empty()) {
    throw std::runtime_error("If a storageFile is set, it may not be empty");
  }
  if(tokenExpiration == decltype(tokenExpiration)::zero()) {
    throw std::runtime_error("tokenExpiration must be set");
  }
  if(oauthTokenSecret.empty()) {
    throw std::runtime_error("oauthTokenSecret must be set");
  }
  if (signingIdentity == nullptr) {
    throw std::runtime_error("signingIdentity must be set");
  }
}
const std::unordered_map<std::string, std::string> checksumNameMappings{
    {"groups", "user-groups"}, {"user-groups-v2", "user-group-users-legacy"}};

AuthserverBackend::AuthserverBackend(const Parameters &params)
    : MessageSigner(params.getSigningIdentity()),
      mAccessManager(std::make_shared<AccessManagerProxy>(params.getAccessManager(), *this)),
      mTokenExpiration(params.getTokenExpiration()),
      mOauthTokenSecret(params.getOAuthTokenSecret()){
  if (params.getStorageFile() && std::filesystem::exists(*params.getStorageFile())) {
    migrateDatabase(*params.getStorageFile());
  }
}

std::vector<std::string> AuthserverBackend::getChecksumChainNames() const {
  std::vector<std::string> checksumChainNames;
  std::ranges::transform(checksumNameMappings, std::back_inserter(checksumChainNames), [](const auto& pair) { return pair.first; });
  return checksumChainNames;
}

rxcpp::observable<ChecksumChainResponse> AuthserverBackend::handleChecksumChainRequest(ChecksumChainRequest request) {
  // authserver used to have it's own storage, but that has been moved to the
  // access manager. To make sure the migration from authserver to access
  // manager goes correctly, we keep the old checksum chains, but instead of
  // calculating them here, we pass them on to the accessmanager.
  //TODO: When the migration has succeeded, this can be removed in a following release.
  auto checksumMapping = checksumNameMappings.find(request.mName);
  if (checksumMapping == checksumNameMappings.end()) {
    throw Error("Checksum chain " + request.mName + " not found");
  }
  request.mName = checksumMapping->second;
  return mAccessManager->requestChecksumChain(std::move(request))
    .op(RxGetOne());
}

rxcpp::observable<std::optional<std::vector<UserGroup>>> AuthserverBackend::findUserGroupsAndStorePrimaryIdIfMissing(
    const std::string &primaryId,
    const std::vector<std::string> &alternativeIds) {

  return mAccessManager->findUser(primaryId, alternativeIds)
    .map([](FindUserResponse response) {
      return response.mUserGroups;
    });
}


OAuthToken AuthserverBackend::getToken(const std::string& uid, const std::string& group, const Timestamp& expirationTime) const {
  auto now = TimeNow<std::chrono::sys_seconds>();
  return OAuthToken::Generate(
    mOauthTokenSecret, uid, group,
    now,
    time_point_cast<std::chrono::seconds>(expirationTime)
  );
}

OAuthToken AuthserverBackend::getToken(const std::string& uid, const UserGroup& group, const std::optional<std::chrono::seconds>& longLivedValidity) const {
  auto maxValidity = group.mMaxAuthValidity;

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
    if(group.mMaxAuthValidity) {
      validity = std::min(this->mTokenExpiration, *group.mMaxAuthValidity);
    }
    else {
      validity = this->mTokenExpiration;
    }
  }
  auto now = TimeNow<std::chrono::sys_seconds>();
  return OAuthToken::Generate(
    mOauthTokenSecret, uid, group.mName,
    now,
    now + validity
  );
}

TokenResponse AuthserverBackend::executeTokenRequest(const std::string& accessGroup, const TokenRequest& request) {
  // Check access
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, accessGroup);

  auto token = getToken(request.mSubject, request.mGroup, request.mExpirationTime);

  return TokenResponse(token.getSerializedForm());
}

void AuthserverBackend::migrateDatabase(const std::filesystem::path& storageFile) {
  LOG(LOG_TAG, info) << "Found authserver storage file. Migrating it to access manager";
  // Because we send the database as a multi-part message, it will not be retried automatically
  // when the connection to the access manager fails. Therefore, we first wait for the connection
  // to succeed, before starting the migration.
  mAccessManager->connectionStatus()
    .filter([](const ConnectionStatus& status){ return status.connected; })
    .first()
    .flat_map([accessManager=mAccessManager, storageFile](ConnectionStatus status) {
      auto storageStream = std::make_shared<std::ifstream>(storageFile, std::ios::binary);
      if (!storageStream->is_open()) {
        throw std::runtime_error("Failed to open storageFile");
      }
      return accessManager->migrateUserDbToAccessManager(messaging::IStreamToMessageBatches(storageStream));
    }).subscribe(
      [](FakeVoid) {
        LOG(LOG_TAG, info) << "Migration successful";
      },
      [](std::exception_ptr e) {
        LOG(LOG_TAG, error) << "Error while trying to migrate authserver storage to access manager: " << rxcpp::rxu::what(e);
      }
    );
}

}
