#include <pep/authserver/Authserver.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

#include <boost/algorithm/hex.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {

static const std::string LOG_TAG ("Authserver");

Authserver::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config),
  backendParams(config), oauthParams(io_context, config) {

  EndPoint accessManagerEndPoint;

  try {
    accessManagerEndPoint = config.get<EndPoint>("AccessManager");
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  backendParams.setAccessManager(messaging::ServerConnection::TryCreate(this->getIoContext(), accessManagerEndPoint, getRootCACertificatesFilePath()));
  backendParams.setCertificateChain(getCertificateChain());
  backendParams.setPrivateKey(getPrivateKey());
}

const AuthserverBackend::Parameters& Authserver::Parameters::getBackendParams() const {
  return backendParams;
}

const OAuthProvider::Parameters& Authserver::Parameters::getOAuthParams() const {
  return oauthParams;
}

void Authserver::Parameters::check() const {
  backendParams.check();
  oauthParams.check();
  SigningServer::Parameters::check();
}

std::vector<std::string> Authserver::getChecksumChainNames() const {
  return mBackend->getChecksumChainNames();
}

void Authserver::computeChecksumChainChecksum(
    const std::string& chain,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) {
  // Because we override handleChecksumChainRequest, this method is never called.
  // TODO: refactor MonitorableServer so that we don't have this method that should never be called.
  throw std::logic_error("computeChecksumChainChecksum should not be called for Authserver");
}

messaging::MessageBatches Authserver::handleTokenRequest(std::shared_ptr<SignedTokenRequest> signedRequest) {
  auto request = signedRequest->open(this->getRootCAs());
  auto accessGroup = signedRequest->getLeafCertificateOrganizationalUnit();
  
  return messaging::BatchSingleMessage(mBackend->executeTokenRequest(accessGroup, request));
}

messaging::MessageBatches Authserver::handleChecksumChainRequest(std::shared_ptr<SignedChecksumChainRequest> signedRequest) {
  UserGroup::EnsureAccess(getAllowedChecksumChainRequesters(), signedRequest->getLeafCertificateOrganizationalUnit(), "Requesting checksum chains");
  auto request = signedRequest->open(getRootCAs());

  return mBackend->handleChecksumChainRequest(request).map([](ChecksumChainResponse response) -> messaging::MessageSequence {
    return rxcpp::rxs::just(std::make_shared<std::string>(Serialization::ToString(response)));
  });
}

Authserver::Authserver(std::shared_ptr<Parameters> parameters)
  : SigningServer(parameters),
  mBackend(std::make_shared<AuthserverBackend>(parameters->getBackendParams())),
  mOAuth(OAuthProvider::Create(parameters->getOAuthParams(), mBackend)) {
  RegisterRequestHandlers(*this,
                          &Authserver::handleTokenRequest,
                          &Authserver::handleChecksumChainRequest); //This overwrites the handler in MonitorableServer with our own handler
}
std::string Authserver::describe() const {
  return "Authserver";
}

}
