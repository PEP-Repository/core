#include <pep/authserver/Authserver.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {

namespace {

const std::string LogTag ("Authserver");

}

Authserver::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config),
  backendParams_(config), oauthParams_(io_context, config) {

  EndPoint accessManagerEndPoint;

  try {
    auto serverEndPoints = config.get_child("ServerEndPoints");
    accessManagerEndPoint = serverEndPoints.get<EndPoint>(ServerTraits::AccessManager().configNode());
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  backendParams_.setAccessManager(messaging::ServerConnection::TryCreate(this->getIoContext(), accessManagerEndPoint, getRootCACertificatesFilePath()));
  backendParams_.setAccessManagerEndPoint(std::move(accessManagerEndPoint));
  backendParams_.setSigningIdentity(getSigningIdentity());
  backendParams_.setRootCertificates(getRootCAs());
}

const AuthserverBackend::Parameters& Authserver::Parameters::getBackendParams() const {
  return backendParams_;
}

const OAuthProvider::Parameters& Authserver::Parameters::getOAuthParams() const {
  return oauthParams_;
}

void Authserver::Parameters::check() const {
  backendParams_.check();
  oauthParams_.check();
  SigningServer::Parameters::check();
}

const ChecksumChainHandler& Authserver::checksumChainHandler() const noexcept {
  return backend_->checksumChainForwarder();
}

messaging::MessageBatches Authserver::handleTokenRequest(std::shared_ptr<SignedTokenRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto accessGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->executeTokenRequest(accessGroup, request));
}

Authserver::Authserver(std::shared_ptr<Parameters> parameters)
  : SigningServer(parameters),
  backend_(std::make_shared<AuthserverBackend>(parameters->getBackendParams())),
  oAuth_(OAuthProvider::Create(parameters->getOAuthParams(), backend_)) {
  RegisterRequestHandlers(*this,
                          &Authserver::handleTokenRequest);
}

}
