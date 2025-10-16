#include <pep/keyserver/KeyServer.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/keyserver/tokenblocking/SqliteBlocklist.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <pep/auth/FacilityType.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {
namespace {

constexpr auto validityTimeOfGeneratedCertificates = std::chrono::hours{12};
const std::string LOG_TAG("KeyServer");

tokenBlocking::TokenIdentifier Identifiers(const OAuthToken& token) {
  return {
      .subject = token.getSubject(),
      .userGroup = token.getGroup(),
      .issueDateTime = token.getIssuedAt(),
  };
}

std::unique_ptr<tokenBlocking::Blocklist> CreateBlocklist(const KeyServer::Parameters& parameters) {
  const auto& path = parameters.getBlocklistStoragePath();
  return path.has_value() ? tokenBlocking::SqliteBlocklist::CreateWithStorageLocation(*path) : nullptr;
}

X509CertificateChain CertificateChainFromHeadAndTail(const X509Certificate& head, const X509CertificateChain& tail) {
  X509CertificateChain chain;
  chain.insert(chain.end(), head);
  chain.insert(chain.end(), tail.begin(), tail.end());
  return chain;
}

std::vector<tokenBlocking::Blocklist::Entry> allEntries(tokenBlocking::Blocklist* list) {
  return list ? list->allEntries() : std::vector<tokenBlocking::Blocklist::Entry>{};
}

void EnsureTokenBlockingAdminAccess(const std::string& organizationalUnit) {
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, organizationalUnit, "token blocklist management");
}

} // namespace

KeyServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(std::move(io_context), config) {
  std::filesystem::path clientCAPrivateKeyFile;
  std::filesystem::path clientCACertificateChainFile;
  std::filesystem::path oauthTokenSecretFile;
  std::optional<std::filesystem::path> blocklistStoragePath;
  try {
    clientCAPrivateKeyFile = config.get<std::filesystem::path>("ClientCAPrivateKeyFile");
    clientCACertificateChainFile = config.get<std::filesystem::path>("ClientCACertificateChainFile");

    oauthTokenSecretFile = canonical(config.get<std::filesystem::path>("OAuthTokenSecretFile"));

    blocklistStoragePath = config.get<std::optional<std::filesystem::path>>("BlocklistStoragePath");
    if (blocklistStoragePath) { blocklistStoragePath = weakly_canonical(*blocklistStoragePath); }
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  // read the oauth secret token from the json file specified in the configuration
  try {
    Configuration oauthProperties = Configuration::FromFile(oauthTokenSecretFile);

    std::string secretHex = oauthProperties.get<std::string>("OAuthTokenSecret");
    setOauthTokenSecret(boost::algorithm::unhex(secretHex));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with oauth file: " << e.what();
    throw;
  }

  setClientCAPrivateKey(AsymmetricKey(ReadFile(clientCAPrivateKeyFile)));
  setClientCACertificateChain(X509CertificateChain(ReadFile(clientCACertificateChainFile)));
  setBlocklistStoragePath(blocklistStoragePath);
}

const AsymmetricKey& KeyServer::Parameters::getClientCAPrivateKey() const { return mClientCAPrivateKey; }

void KeyServer::Parameters::setClientCAPrivateKey(const AsymmetricKey& privateKey) {
  Parameters::mClientCAPrivateKey = privateKey;
}

X509CertificateChain& KeyServer::Parameters::getClientCACertificateChain() { return mClientCACertificateChain; }

void KeyServer::Parameters::setClientCACertificateChain(const X509CertificateChain& certificateChain) {
  Parameters::mClientCACertificateChain = certificateChain;
}

const std::string& KeyServer::Parameters::getOauthTokenSecret() const { return mOauthTokenSecret; }

void KeyServer::Parameters::setOauthTokenSecret(const std::string& oauthTokenSecret) {
  Parameters::mOauthTokenSecret = oauthTokenSecret;
}

const std::optional<std::filesystem::path>& KeyServer::Parameters::getBlocklistStoragePath() const {
  return mBlocklistStoragePath;
}

void KeyServer::Parameters::setBlocklistStoragePath(const std::optional<std::filesystem::path>& blocklistStoragePath) {
  Parameters::mBlocklistStoragePath = blocklistStoragePath;
}

void KeyServer::Parameters::check() const {
  if (!mClientCAPrivateKey.isSet()) throw std::runtime_error("clientCAPrivateKey must be set");
  if (mClientCACertificateChain.empty()) throw std::runtime_error("clientCACertificateChain must not be empty");
  if (mOauthTokenSecret.empty()) throw std::runtime_error("oauthTokenSecret must not be empty");
  Server::Parameters::check();
}

messaging::MessageBatches KeyServer::handleUserEnrollmentRequest(
    std::shared_ptr<EnrollmentRequest> enrollmentRequest) {
  checkValid(*enrollmentRequest);
  const auto certificate = generateCertificate(enrollmentRequest->mCertificateSigningRequest);

  EnrollmentResponse response;
  response.mCertificateChain = CertificateChainFromHeadAndTail(certificate, mClientCACertificateChain);
  LOG(LOG_TAG, debug) << "Sending certificate chain len=" << response.mCertificateChain.size() << ":"
                      << response.mCertificateChain.toPem();
  return messaging::BatchSingleMessage(std::move(response));
}

messaging::MessageBatches KeyServer::handleTokenBlockingListRequest(
    std::shared_ptr<SignedTokenBlockingListRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->getLeafCertificateOrganizationalUnit());

  TokenBlockingListResponse response;
  response.entries = allEntries(mBlocklist.get());
  return messaging::BatchSingleMessage(std::move(response));
}

messaging::MessageBatches KeyServer::handleTokenBlockingCreateRequest(
    std::shared_ptr<SignedTokenBlockingCreateRequest> signedRequest) {
   UserGroup::EnsureAccess({UserGroup::AccessAdministrator, UserGroup::AccessManager}, signedRequest->getLeafCertificateOrganizationalUnit(), "token blocklist management");
  auto request = signedRequest->open(this->getRootCAs());
  if (mBlocklist == nullptr) { throw Error{"KeyServer does not have a blocklist"}; }

  auto entry = tokenBlocking::BlocklistEntry{
      .id = 0,
      .target = std::move(request.target),
      .metadata{
          .note = std::move(request.note),
          .issuer = signedRequest->getLeafCertificateCommonName(),
          .creationDateTime = Timestamp::now()}};
  entry.id = mBlocklist->add(entry.target, entry.metadata);
  return messaging::BatchSingleMessage(TokenBlockingCreateResponse{std::move(entry)});
}

messaging::MessageBatches KeyServer::handleTokenBlockingRemoveRequest(
    std::shared_ptr<SignedTokenBlockingRemoveRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->getLeafCertificateOrganizationalUnit());
  const auto request = signedRequest->open(this->getRootCAs());
  if (mBlocklist == nullptr) { throw Error{"KeyServer does not have a blocklist"}; }

  auto entry = mBlocklist->removeById(request.id);
  if (!entry.has_value()) { throw Error{"Entry with id=" + std::to_string(request.id) + " does not exist."}; }
  return messaging::BatchSingleMessage(TokenBlockingRemoveResponse{std::move(*entry)});
}

KeyServer::KeyServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters),
    mClientCAPrivateKey(parameters->getClientCAPrivateKey()),
    mClientCACertificateChain(parameters->getClientCACertificateChain()),
    mOauthTokenSecret(parameters->getOauthTokenSecret()),
    mBlocklist(CreateBlocklist(*parameters)) {
  RegisterRequestHandlers(
      *this,
      &KeyServer::handleUserEnrollmentRequest,
      &KeyServer::handleTokenBlockingListRequest,
      &KeyServer::handleTokenBlockingCreateRequest,
      &KeyServer::handleTokenBlockingRemoveRequest);
}

std::string KeyServer::describe() const { return "Key Server"; }

void KeyServer::checkValid(const EnrollmentRequest& request) const {

  if(!request.mCertificateSigningRequest.getCommonName().has_value()) {
    throw Error("Certificate does not contain a common name for user enrollment request");
  }
  auto cn = request.mCertificateSigningRequest.getCommonName().value();

  if(!request.mCertificateSigningRequest.getOrganizationalUnit().has_value()) {
    throw Error("Certificate does not contain an organizational unit for user enrollment request");
  }
  auto ou = request.mCertificateSigningRequest.getOrganizationalUnit().value();

  if (CertificateSubjectToFacilityType(cn, ou) != FacilityType::Unknown) {
    throw Error("Invalid certificate subject for user enrollment request");
  }

  const auto token = OAuthToken::Parse(request.mOAuthToken);
  if (!isValid(token, cn, ou)) { throw Error("OAuth token invalid"); }
  LOG(LOG_TAG, debug) << "Checked OAuth ticket for " << cn << " in group " << ou;

  if (request.mCertificateSigningRequest.verifySignature() == false) {
    throw Error("Could not verify CSR signature");
  }
}

bool KeyServer::isValid(
    const OAuthToken& authToken,
    const std::string& commonName,
    const std::string& organizationalUnit) const {
  const auto isBlocked = [this](const OAuthToken& token) {
    if (mBlocklist == nullptr) {
      LOG(LOG_TAG, debug) << "Skipping blocklist check as no blocklist was provided";
      return false;
    }

    LOG(LOG_TAG, debug) << "Checking token against blocklist";
    const auto blocked = IsBlocking(*mBlocklist, Identifiers(token));
    if (blocked) { LOG(LOG_TAG, info) << "Token is blocked and therefore considered invalid"; }
    return blocked;
  };

  const auto isProper = authToken.verify(mOauthTokenSecret, commonName, organizationalUnit);
  return isProper && !isBlocked(authToken);
}

X509Certificate KeyServer::generateCertificate(const pep::X509CertificateSigningRequest& csr) const {
  try {
    const auto certificate = csr.signCertificate(
        mClientCACertificateChain.empty() ? X509Certificate() : *mClientCACertificateChain.begin(),
        mClientCAPrivateKey,
        validityTimeOfGeneratedCertificates);
    assert(GetFacilityType(certificate) == FacilityType::User);
    LOG(LOG_TAG, debug) << "Generated certificate for CN=" << csr.getCommonName().value_or("")
                        << " in OU=" << csr.getOrganizationalUnit().value_or("");
    return certificate;
  }
  catch (...) {
    throw Error{"Certificate generation failed"};
  }
}

} // namespace pep
