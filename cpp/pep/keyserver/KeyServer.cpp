#include <pep/keyserver/KeyServer.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/keyserver/tokenblocking/SqliteBlocklist.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <pep/auth/ServerTraits.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {
namespace {

constexpr auto validityTimeOfGeneratedCertificates = std::chrono::hours{12};
const std::string LogTag("KeyServer");

tokenBlocking::TokenIdentifier Identifiers(const OAuthToken& token) {
  return {
      .subject = token.getSubject(),
      .userGroup = token.getGroup(),
      .issueDateTime = token.getIssuedAt(),
  };
}

void EnsureTokenBlockingAdminAccess(const std::string& organizationalUnit) {
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, organizationalUnit, "token blocklist management");
}

} // namespace

KeyServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(std::move(io_context), config) {
  std::filesystem::path oauthTokenSecretFile;
  std::filesystem::path blocklistStoragePath;
  try {
    clientCa_ = std::move(*X509IdentityFiles::FromConfig(config.get_child("ClientCa"), *getRootCAs()).identity());

    oauthTokenSecretFile = canonical(config.get<std::filesystem::path>("OAuthTokenSecretFile"));

    blocklistStoragePath = weakly_canonical(config.get<std::filesystem::path>("BlocklistStoragePath"));
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  // read the oauth secret token from the json file specified in the configuration
  try {
    Configuration oauthProperties = Configuration::FromFile(oauthTokenSecretFile);

    std::string secretHex = oauthProperties.get<std::string>("OAuthTokenSecret");
    setOauthTokenSecret(boost::algorithm::unhex(secretHex));
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with oauth file: " << e.what();
    throw;
  }

  setBlocklistStoragePath(blocklistStoragePath);
}


const std::string& KeyServer::Parameters::getOauthTokenSecret() const { return oauthTokenSecret_; }

void KeyServer::Parameters::setOauthTokenSecret(const std::string& oauthTokenSecret) {
  Parameters::oauthTokenSecret_ = oauthTokenSecret;
}

const std::filesystem::path& KeyServer::Parameters::getBlocklistStoragePath() const {
  return blocklistStoragePath_;
}

void KeyServer::Parameters::setBlocklistStoragePath(const std::filesystem::path& blocklistStoragePath) {
  Parameters::blocklistStoragePath_ = blocklistStoragePath;
}

void KeyServer::Parameters::check() const {
  if (!clientCa_.has_value()) throw std::runtime_error("clientCa must be set");
  if (oauthTokenSecret_.empty()) throw std::runtime_error("oauthTokenSecret must not be empty");
  if (blocklistStoragePath_.empty()) throw std::runtime_error("blocklistStoragePath must not be empty");
  Server::Parameters::check();
}

messaging::MessageBatches KeyServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  return messaging::BatchSingleMessage(Serialization::ToString(PingResponse(request->id())));
}

messaging::MessageBatches KeyServer::handleUserEnrollmentRequest(
    std::shared_ptr<EnrollmentRequest> enrollmentRequest) {
  checkValid(*enrollmentRequest);
  const auto certificate = generateCertificate(enrollmentRequest->certificateSigningRequest);

  EnrollmentResponse response{
    .certificateChain = clientCa_.getCertificateChain() / certificate
  };
  PEP_LOG(LogTag, Severity::Debug) << "Sending certificate chain len=" << response.certificateChain.certificates().size() << ":"
                      << X509CertificatesToPem(response.certificateChain.certificates());
  return messaging::BatchSingleMessage(std::move(response));
}

messaging::MessageBatches KeyServer::handleTokenBlockingListRequest(
    std::shared_ptr<SignedTokenBlockingListRequest> signedRequest) {
  EnsureTokenBlockingAdminAccess(signedRequest->validate(*this->getRootCAs()).organizationalUnit());

  TokenBlockingListResponse response;
  response.entries = blocklist_->allEntries();
  return messaging::BatchSingleMessage(std::move(response));
}

messaging::MessageBatches KeyServer::handleTokenBlockingCreateRequest(
    std::shared_ptr<SignedTokenBlockingCreateRequest> signedRequest) {

  auto certified = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator, UserGroup::AccessManager}, certified.signatory.organizationalUnit(), "token blocklist management");
  const auto& request = certified.message;

  assert(blocklist_ != nullptr);

  auto entry = tokenBlocking::BlocklistEntry{
      .id = 0,
      .target = request.target,
      .metadata{
          .note = request.note,
          .issuer = certified.signatory.commonName(),
          .creationDateTime = TimeNow()}};
  entry.id = blocklist_->add(entry.target, entry.metadata);
  return messaging::BatchSingleMessage(TokenBlockingCreateResponse{std::move(entry)});
}

messaging::MessageBatches KeyServer::handleTokenBlockingRemoveRequest(
    std::shared_ptr<SignedTokenBlockingRemoveRequest> signedRequest) {
  const auto certified = signedRequest->open(*this->getRootCAs());
  EnsureTokenBlockingAdminAccess(certified.signatory.organizationalUnit());
  const auto& request = certified.message;

  assert(blocklist_ != nullptr);

  auto entry = blocklist_->removeById(request.id);
  if (!entry.has_value()) { throw Error{"Entry with id=" + std::to_string(request.id) + " does not exist."}; }
  return messaging::BatchSingleMessage(TokenBlockingRemoveResponse{std::move(*entry)});
}

KeyServer::KeyServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters),
    clientCa_(*parameters->getClientCa()),
    oauthTokenSecret_(parameters->getOauthTokenSecret()),
    blocklist_(tokenBlocking::SqliteBlocklist::CreateWithStorageLocation(parameters->getBlocklistStoragePath())) {
  RegisterRequestHandlers(
      *this,
      &KeyServer::handlePingRequest,
      &KeyServer::handleUserEnrollmentRequest,
      &KeyServer::handleTokenBlockingListRequest,
      &KeyServer::handleTokenBlockingCreateRequest,
      &KeyServer::handleTokenBlockingRemoveRequest);
}

void KeyServer::checkValid(const EnrollmentRequest& request) const {

  if(!request.certificateSigningRequest.getCommonName().has_value()) {
    throw Error("Certificate does not contain a common name for user enrollment request");
  }
  auto cn = request.certificateSigningRequest.getCommonName().value();

  if(!request.certificateSigningRequest.getOrganizationalUnit().has_value()) {
    throw Error("Certificate does not contain an organizational unit for user enrollment request");
  }
  auto ou = request.certificateSigningRequest.getOrganizationalUnit().value();

  if (ServerTraits::Find([ou](const ServerTraits& candidate) {return candidate.enrollmentSubject(false) == ou; })) {
    throw Error("Can't enroll user into server group " + ou);
  }

  const auto token = OAuthToken::Parse(request.oAuthToken);
  if (!isValid(token, cn, ou)) { throw Error("OAuth token invalid"); }
  PEP_LOG(LogTag, Severity::Debug) << "Checked OAuth ticket for " << cn << " in group " << ou;

  if (request.certificateSigningRequest.verifySignature() == false) {
    throw Error("Could not verify CSR signature");
  }
}

bool KeyServer::isValid(
    const OAuthToken& authToken,
    const std::string& commonName,
    const std::string& organizationalUnit) const {
  const auto isBlocked = [this](const OAuthToken& token) {
    assert(blocklist_ != nullptr);

    PEP_LOG(LogTag, Severity::Debug) << "Checking token against blocklist";
    const auto blocked = IsBlocking(*blocklist_, Identifiers(token));
    if (blocked) { PEP_LOG(LogTag, Severity::Info) << "Token is blocked and therefore considered invalid"; }
    return blocked;
  };

  const auto isProper = authToken.verify(oauthTokenSecret_, commonName, organizationalUnit);
  return isProper && !isBlocked(authToken);
}

X509Certificate KeyServer::generateCertificate(const pep::X509CertificateSigningRequest& csr) const {
  const auto certificate = csr.signCertificate(
      clientCa_.getCertificateChain().leaf(),
      clientCa_.getPrivateKey(),
      validityTimeOfGeneratedCertificates);
  assert(GetEnrolledParty(certificate) == EnrolledParty::User);
  PEP_LOG(LogTag, Severity::Debug) << "Generated certificate for CN=" << csr.getCommonName().value_or("")
                      << " in OU=" << csr.getOrganizationalUnit().value_or("");
  return certificate;
}

} // namespace pep
