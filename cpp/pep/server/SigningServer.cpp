#include <pep/auth/Certificate.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), MessageSigner(parameters->getSigningIdentity()), identityFiles_(parameters->getIdentityFilesConfig()) {
  RegisterRequestHandlers(*this,
    &SigningServer::handlePingRequest,
    &SigningServer::handleCsrRequest,
    &SigningServer::handleCertificateReplacementRequest,
    &SigningServer::handleCertificateReplacementCommitRequest);
}

messaging::MessageBatches SigningServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(PingResponse(request->id()))));
}

messaging::MessageBatches SigningServer::handleCsrRequest(std::shared_ptr<SignedCsrRequest> signedRequest) {
  auto signatory = signedRequest->validate(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::SystemAdministrator}, signatory.organizationalUnit(), "Requesting CSRs");

  AsymmetricKeyPair newKeyPair = AsymmetricKeyPair::GenerateKeyPair();
  newPrivateKey_ = newKeyPair.getPrivateKey();
  CsrResponse response(
  X509CertificateSigningRequest::CreateWithSubjectFromExistingCertificate(newKeyPair, identityFiles_->identity()->getCertificateChain().leaf()));
  return messaging::BatchSingleMessage(Serialization::ToString(sign(response)));
}

messaging::MessageBatches SigningServer::handleCertificateReplacementRequest(std::shared_ptr<SignedCertificateReplacementRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::SystemAdministrator}, certified.signatory.organizationalUnit(), "Renewing certificates");

  const auto& request = certified.message;

  if (!newPrivateKey_) {
    throw Error("Cannot replace certificate for server, since the server does not have a new private key.");
  }

  if (!request.getCertificateChain().certifiesPrivateKey(*newPrivateKey_)) {
    throw Error("Cannot replace certificate for server, since the certificate does not match the new private key of the server.");
  }

  if (request.getCertificateChain().leaf().hasTLSServerEKU()) {
    throw Error("Cannot replace certificate for server, since it is a TLS certificate. It must be a PEP certificate.");
  }

  if (!request.allowChangingSubject() && !request.getCertificateChain().leaf().hasSameSubject(identityFiles_->identity()->getCertificateChain().leaf())) {
    throw Error("New certificate has a different subject from the current certificate. Use --allow-changing-subject to force replacing the certificate.");
  }

  auto traits = getServerTraits();
  if (!traits.signingIdentityMatches(request.getCertificateChain())) {
    throw Error("Signing identity of the new certificate does not match that of the server");
  }

  if (!IsServerSigningCertificate(identityFiles_->identity()->getCertificateChain().leaf())) {
    throw std::runtime_error("New certificate is not a server signing certificate");
  }

  if (!request.getCertificateChain().verify(*getRootCAs())) {
    throw Error("Cannot replace certificate for server, since the new certificate chain cannot be verified.");
  }

  *identityFiles_->identity() = X509Identity(*newPrivateKey_, request.getCertificateChain());

  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(CertificateReplacementResponse()))); //Sign with the new certificate, so requestor can check that it is correctly deployed
}

messaging::MessageBatches SigningServer::handleCertificateReplacementCommitRequest(std::shared_ptr<SignedCertificateReplacementCommitRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::SystemAdministrator}, certified.signatory.organizationalUnit(), "Committing renewed certificates");

  const auto& request = certified.message;

  if (request.getCertificateChain() != identityFiles_->identity()->getCertificateChain()) {
    throw Error("Cannot commit replaced certificate for server, since the certificate chain in the request does not match the current certificate chain of the server");
  }

  if (!newPrivateKey_) {
    throw Error("Cannot commit replaced certificate for server, since the server does not have a new private key.");
  }

  if (identityFiles_->identity()->getPrivateKey() != *newPrivateKey_) {
    throw Error("Cannot commit the certificate and private key that are currently in use, because the current private key is different from the new private key.");
  }

  WriteFile(identityFiles_->getPrivateKeyFilePath(), identityFiles_->identity()->getPrivateKey().toPem());
  WriteFile(identityFiles_->getCertificateChainFilePath(), X509CertificatesToPem(identityFiles_->identity()->getCertificateChain().certificates()));
  newPrivateKey_ = std::nullopt;

  return messaging::BatchSingleMessage(Serialization::ToString(CertificateReplacementCommitResponse()));
}

SigningServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(io_context, config), identityFiles_(MakeSharedCopy(X509IdentityFiles::FromConfig(config.get_child("SigningIdentity"), *getRootCAs()))) {
}

}
