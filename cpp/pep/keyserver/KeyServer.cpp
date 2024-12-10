#include <pep/keyserver/KeyServer.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/keyserver/tokenblocking/SqliteBlocklist.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <pep/auth/FacilityType.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/auth/UserGroups.hpp>

namespace pep {
namespace {

constexpr auto validityTimeOfGeneratedCertificates = std::chrono::hours{12};
const std::string LOG_TAG("KeyServer");

tokenBlocking::TokenIdentifier Identifiers(const OAuthToken& token) {
  return {
      .subject = token.getSubject(),
      .userGroup = token.getGroup(),
      .issueDateTime = pep::Timestamp{token.getIssuedAt()}};
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
  user_group::EnsureAccess({user_group::AccessAdministrator}, organizationalUnit, "token blocklist management");
}

} // namespace

KeyServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config)
  : Server::Parameters(io_service, config) {
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

  setClientCAPrivateKey(ReadFile(clientCAPrivateKeyFile));
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

void KeyServer::Parameters::check() {
  if (!mClientCAPrivateKey.isSet()) throw std::runtime_error("clientCAPrivateKey must be set");
  if (mClientCACertificateChain.empty()) throw std::runtime_error("clientCACertificateChain must not be empty");
  if (mOauthTokenSecret.empty()) throw std::runtime_error("oauthTokenSecret must not be empty");
  Server::Parameters::check();
}

KeyServer::SerializedResponse KeyServer::handleUserEnrollmentRequest(
    std::shared_ptr<EnrollmentRequest> enrollmentRequest) {
  checkValid(*enrollmentRequest);
  const auto certificate = generateCertificate(enrollmentRequest->mCertificateSigningRequest);

  EnrollmentResponse response;
  response.mCertificateChain = CertificateChainFromHeadAndTail(certificate, mClientCACertificateChain);
  LOG(LOG_TAG, debug) << "Sending certificate chain len=" << response.mCertificateChain.size() << ":"
                      << response.mCertificateChain.toPem();
  return Just(std::move(response));
}

KeyServer::SerializedResponse KeyServer::handleTokenBlockingListRequest(
    std::shared_ptr<SignedTokenBlockingListRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->getOrganizationalUnit());

  TokenBlockingListResponse response;
  response.entries = allEntries(mBlocklist.get());
  return Just(response);
}

KeyServer::SerializedResponse KeyServer::handleTokenBlockingCreateRequest(
    std::shared_ptr<SignedTokenBlockingCreateRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->getOrganizationalUnit());
  auto request = signedRequest->open(this->getRootCAs());
  if (mBlocklist == nullptr) { throw Error{"KeyServer does not have a blocklist"}; }

  auto entry = tokenBlocking::BlocklistEntry{
      .id = 0,
      .target = std::move(request.target),
      .metadata{
          .note = std::move(request.note),
          .issuer = signedRequest->getCommonName(),
          .creationDateTime = Timestamp{}}};
  entry.id = mBlocklist->add(entry.target, entry.metadata);
  return Just(TokenBlockingCreateResponse{std::move(entry)});
}

KeyServer::SerializedResponse KeyServer::handleTokenBlockingRemoveRequest(
    std::shared_ptr<SignedTokenBlockingRemoveRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->getOrganizationalUnit());
  const auto request = signedRequest->open(this->getRootCAs());
  if (mBlocklist == nullptr) { throw Error{"KeyServer does not have a blocklist"}; }

  auto entry = mBlocklist->removeById(request.id);
  if (!entry.has_value()) { throw Error{"Entry with id=" + std::to_string(request.id) + " does not exist."}; }
  return Just(TokenBlockingRemoveResponse{std::move(*entry)});
}

KeyServer::KeyServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters),
    mClientCAPrivateKey(parameters->getClientCAPrivateKey()),
    mClientCACertificateChain(parameters->getClientCACertificateChain()),
    mOauthTokenSecret(parameters->getOauthTokenSecret()),
    mBlocklist(CreateBlocklist(*parameters)) {
  registerRequestHandlers(
      this,
      &KeyServer::handleUserEnrollmentRequest,
      &KeyServer::handleTokenBlockingListRequest,
      &KeyServer::handleTokenBlockingCreateRequest,
      &KeyServer::handleTokenBlockingRemoveRequest);
}

std::string KeyServer::describe() const { return "Key Server"; }

void KeyServer::checkValid(const EnrollmentRequest& request) const {
  const auto cn = request.mCertificateSigningRequest.getCommonName();
  const auto ou = request.mCertificateSigningRequest.getOrganizationalUnit();
  if (CertificateSubjectToFacilityType(cn, ou) != FacilityType::Unknown) {
    throw Error("Invalid certificate subject for user enrollment request");
  }

  const auto token = OAuthToken::Parse(request.mOAuthToken);
  if (!isValid(token, cn, ou)) { throw Error("OAuth token invalid"); }
  LOG(LOG_TAG, debug) << "Checked OAuth ticket for " << cn << " in group " << ou;

  try {
    request.mCertificateSigningRequest.verifySignature();
  }
  catch (...) {
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
    constexpr auto validityInSeconds = std::chrono::seconds{validityTimeOfGeneratedCertificates}.count();
    const auto certificate = csr.signCertificate(
        mClientCACertificateChain.empty() ? X509Certificate() : *mClientCACertificateChain.begin(),
        mClientCAPrivateKey,
        validityInSeconds);
    assert(GetFacilityType(certificate) == FacilityType::User);
    LOG(LOG_TAG, debug) << "Generated certificate for CN=" << csr.getCommonName()
                        << " in OU=" << csr.getOrganizationalUnit();
    return certificate;
  }
  catch (...) {
    throw Error{"Certificate generation failed"};
  }
}

} // namespace pep
