#include <pep/authserver/Authserver.hpp>
#include <pep/authserver/AsaSerializers.hpp>

namespace pep {

Authserver::Parameters::Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config)
  : SigningServer::Parameters(io_service, config),
  backendParams(config),
  oauthParams(io_service, config) {
}

const AuthserverBackend::Parameters& Authserver::Parameters::getBackendParams() const {
  return backendParams;
}

const OAuthProvider::Parameters& Authserver::Parameters::getOAuthParams() const {
  return oauthParams;
}

void Authserver::Parameters::check() {
  backendParams.check();
  oauthParams.check();
  SigningServer::Parameters::check();
}

std::vector<std::string> Authserver::getChecksumChainNames() const {
  return backend->getChecksumChainNames();
}

void Authserver::computeChecksumChainChecksum(
    const std::string& chain,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) {
  backend->computeChecksumChainChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> Authserver::handleAsaQuery(std::shared_ptr<SignedAsaQuery> signedRequest) {
  auto request = signedRequest->open(this->getRootCAs());
  auto accessGroup = signedRequest->getOrganizationalUnit();

  return Just(backend->executeAsaQuery(accessGroup, request));
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> Authserver::handleAsaMutationRequest(std::shared_ptr<SignedAsaMutationRequest> signedRequest) {
  auto request = signedRequest->open(this->getRootCAs());
  auto accessGroup = signedRequest->getOrganizationalUnit();

  return Just(backend->executeAsaMutationRequest(accessGroup, request));
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> Authserver::handleAsaTokenRequest(std::shared_ptr<SignedAsaTokenRequest> signedRequest) {
  auto request = signedRequest->open(this->getRootCAs());
  auto accessGroup = signedRequest->getOrganizationalUnit();

  return Just(backend->executeAsaTokenRequest(accessGroup, request));
}

Authserver::Authserver(std::shared_ptr<Parameters> parameters)
  : SigningServer(parameters),
  backend(std::make_shared<AuthserverBackend>(parameters->getBackendParams())),
  oAuth(std::make_shared<OAuthProvider>(parameters->getOAuthParams(), backend)) {
  registerRequestHandlers(this,
                          &Authserver::handleAsaQuery,
                          &Authserver::handleAsaMutationRequest,
                          &Authserver::handleAsaTokenRequest);
}
std::string Authserver::describe() const {
  return "Authserver";
}

std::optional<std::filesystem::path> Authserver::getStoragePath() {
  std::filesystem::path path = ensureDirectoryPath(backend->getStoragePath().parent_path());
  if(!path.empty())
    return path;
  return {};
}
}
