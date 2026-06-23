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

namespace {

const std::string LogTag("AuthserverBackend");

}

AuthserverBackend::Parameters::Parameters(const Configuration& config) {
  std::filesystem::path oauthTokenSecretFile;
  try {
    tokenExpiration_ = std::chrono::seconds(config.get<unsigned int>("TokenExpirationSeconds"));
    oauthTokenSecretFile = std::filesystem::canonical(config.get<std::filesystem::path>("OAuthTokenSecretFile"));

    storageFile_ = config.get<std::optional<std::filesystem::path>>("StorageFile");
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    Configuration secretFile = Configuration::FromFile(oauthTokenSecretFile);

    std::string secretHex = secretFile.get<std::string>("OAuthTokenSecret");
    oauthTokenSecret_ = boost::algorithm::unhex(secretHex);
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with oauth file: " << e.what();
    throw;
  }
}

const EndPoint& AuthserverBackend::Parameters::getAccessManagerEndPoint() const {
  return accessManagerEndpoint_;
}
void AuthserverBackend::Parameters::setAccessManagerEndPoint(EndPoint endPoint) {
  this->accessManagerEndpoint_ = std::move(endPoint);
}

std::shared_ptr<messaging::ServerConnection>
AuthserverBackend::Parameters::getAccessManager() const {
  return accessManager_;
}

void AuthserverBackend::Parameters::setAccessManager(
    std::shared_ptr<messaging::ServerConnection> accessManager) {
  accessManager_ = accessManager;
}

std::shared_ptr<const X509Identity> AuthserverBackend::Parameters::getSigningIdentity() const {
  return signingIdentity_;
}
void AuthserverBackend::Parameters::setSigningIdentity(std::shared_ptr<const X509Identity> identity) {
  signingIdentity_ = identity;
}

std::chrono::seconds AuthserverBackend::Parameters::getTokenExpiration() const {
  return tokenExpiration_;
}

const std::string& AuthserverBackend::Parameters::getOAuthTokenSecret() const { return oauthTokenSecret_; }

const std::optional<std::filesystem::path>& AuthserverBackend::Parameters::getStorageFile() const {return storageFile_; }

std::shared_ptr<X509RootCertificates> AuthserverBackend::Parameters::getRootCertificates() const { return rootCertificates_; }

void AuthserverBackend::Parameters::setRootCertificates(std::shared_ptr<X509RootCertificates> rootCertificates) { rootCertificates_ = std::move(rootCertificates); }

void AuthserverBackend::Parameters::check() const {
  if (!accessManager_) {
    throw std::runtime_error("AccessManager must be set");
  }
  if (accessManagerEndpoint_.expectedCommonName.empty() || accessManagerEndpoint_.hostname.empty() || accessManagerEndpoint_.port == 0) {
    throw std::runtime_error("accessManagerEndpoint_ must be set");
  }
  if(storageFile_ && storageFile_->empty()) {
    throw std::runtime_error("If a storageFile_ is set, it may not be empty");
  }
  if(tokenExpiration_ == decltype(tokenExpiration_)::zero()) {
    throw std::runtime_error("tokenExpiration_ must be set");
  }
  if(oauthTokenSecret_.empty()) {
    throw std::runtime_error("oauthTokenSecret_ must be set");
  }
  if (signingIdentity_ == nullptr) {
    throw std::runtime_error("signingIdentity must be set");
  }
  if (!rootCertificates_) {
    throw std::runtime_error("rootCertificates must be set");
  }
  if (rootCertificates_->items().empty()) {
    throw std::runtime_error("rootCertificates must not be empty");
  }
}
const std::unordered_map<std::string, std::string> checksumNameMappings{
    {"groups", "user-groups"}, {"user-groups-v2", "user-group-users-legacy"}};

AuthserverBackend::AuthserverBackend(const Parameters &params)
    : MessageSigner(params.getSigningIdentity()),
      accessManager_(std::make_shared<AccessManagerProxy>(params.getAccessManager(), *this, params.getAccessManagerEndPoint().expectedCommonName, params.getRootCertificates())),
      tokenExpiration_(params.getTokenExpiration()),
      oauthTokenSecret_(params.getOAuthTokenSecret()){
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
  auto checksumMapping = checksumNameMappings.find(request.name_);
  if (checksumMapping == checksumNameMappings.end()) {
    throw Error("Checksum chain " + request.name_ + " not found");
  }
  request.name_ = checksumMapping->second;
  return accessManager_->requestChecksumChain(std::move(request))
    .op(RxGetOne());
}

rxcpp::observable<std::optional<std::vector<UserGroup>>> AuthserverBackend::findUserGroupsAndStorePrimaryIdIfMissing(
    const std::string &primaryId,
    const std::vector<std::string> &alternativeIds) {

  return accessManager_->findUser(primaryId, alternativeIds)
    .map([](FindUserResponse response) {
      return response.userGroups;
    });
}


OAuthToken AuthserverBackend::getToken(const std::string& uid, const std::string& group, const Timestamp& expirationTime) const {
  auto now = TimeNow<std::chrono::sys_seconds>();
  return OAuthToken::Generate(
    oauthTokenSecret_, uid, group,
    now,
    time_point_cast<std::chrono::seconds>(expirationTime)
  );
}

OAuthToken AuthserverBackend::getToken(const std::string& uid, const UserGroup& group, const std::optional<std::chrono::seconds>& longLivedValidity) const {
  auto maxValidity = group.maxAuthValidity_;

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
    if(group.maxAuthValidity_) {
      validity = std::min(this->tokenExpiration_, *group.maxAuthValidity_);
    }
    else {
      validity = this->tokenExpiration_;
    }
  }
  auto now = TimeNow<std::chrono::sys_seconds>();
  return OAuthToken::Generate(
    oauthTokenSecret_, uid, group.name_,
    now,
    now + validity
  );
}

TokenResponse AuthserverBackend::executeTokenRequest(const std::string& accessGroup, const TokenRequest& request) {
  // Check access
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, accessGroup);

  auto token = getToken(request.subject, request.group, request.expirationTime);

  return TokenResponse{
    .token = token.getSerializedForm()
  };
}

void AuthserverBackend::migrateDatabase(const std::filesystem::path& storageFile_) {
  PEP_LOG(LogTag, Severity::Info) << "Found authserver storage file. Migrating it to access manager";
  // Because we send the database as a multi-part message, it will not be retried automatically
  // when the connection to the access manager fails. Therefore, we first wait for the connection
  // to succeed, before starting the migration.
  accessManager_->connectionStatus()
    .filter([](const ConnectionStatus& status){ return status.connected; })
    .first()
    .flat_map([accessManager=accessManager_, storageFile_](ConnectionStatus status) {
      auto storageStream = std::make_shared<std::ifstream>(storageFile_, std::ios::binary);
      if (!storageStream->is_open()) {
        throw std::runtime_error("Failed to open storageFile_");
      }
      return accessManager->migrateUserDbToAccessManager(messaging::IStreamToMessageBatches(storageStream));
    }).subscribe(
      [](FakeVoid) {
        PEP_LOG(LogTag, Severity::Info) << "Migration successful";
      },
      [](std::exception_ptr e) {
        PEP_LOG(LogTag, Severity::Error) << "Error while trying to migrate authserver storage to access manager: " << rxcpp::rxu::what(e);
      }
    );
}

}
