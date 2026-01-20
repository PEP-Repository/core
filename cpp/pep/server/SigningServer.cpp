#include <pep/auth/Certificate.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), MessageSigner(parameters->getSigningIdentity()), mIdentityFiles(parameters->getIdentityFilesConfig()) {
  RegisterRequestHandlers(*this,
    &SigningServer::handlePingRequest,
    &SigningServer::handleCsrRequest,
    &SigningServer::handleCertificateReplacementRequest,
    &SigningServer::handleCertificateReplacementCommitRequest);
}

messaging::MessageBatches SigningServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(PingResponse(request->mId))));
}

messaging::MessageBatches SigningServer::handleCsrRequest(std::shared_ptr<SignedCsrRequest> signedRequest) {
  signedRequest->validate(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, signedRequest->getLeafCertificateOrganizationalUnit(), "Requesting CSRs");

  AsymmetricKeyPair newKeyPair = AsymmetricKeyPair::GenerateKeyPair();
  mNewPrivateKey = newKeyPair.getPrivateKey();
  CsrResponse response(
  X509CertificateSigningRequest::CreateWithSubjectFromExistingCertificate(newKeyPair, mIdentityFiles->identity()->getCertificateChain().leaf()));
  return messaging::BatchSingleMessage(Serialization::ToString(sign(response)));
}

messaging::MessageBatches SigningServer::handleCertificateReplacementRequest(std::shared_ptr<SignedCertificateReplacementRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, signedRequest->getLeafCertificateOrganizationalUnit(), "Renewing certificates");

  if (!mNewPrivateKey) {
    throw Error("Cannot replace certificate for server, since the server does not have a new private key.");
  }

  if (!request.getCertificateChain().certifiesPrivateKey(*mNewPrivateKey)) {
    throw Error("Cannot replace certificate for server, since the certificate does not match the new private key of the server.");
  }

  if (!request.force() && !request.getCertificateChain().leaf().hasSameSubject(mIdentityFiles->identity()->getCertificateChain().leaf())) {
    throw Error("New certificate has a different subject from the current certificate. Use --force to force replacing the certificate.");
  }

  auto traits = getServerTraits();
  if (!traits.signingIdentityMatches(request.getCertificateChain())) {
    throw Error("Signing identity of the new certificate does not match that of the server");
  }

  if (!IsServerSigningCertificate(mIdentityFiles->identity()->getCertificateChain().leaf())) {
    throw std::runtime_error("New certificate is not a server signing certificate");
  }

  if (!request.getCertificateChain().verify(*getRootCAs())) {
    throw Error("Cannot replace certificate for server, since the new certificate chain cannot be verified.");
  }

  *mIdentityFiles->identity() = X509Identity(*mNewPrivateKey, request.getCertificateChain());

  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(CertificateReplacementResponse()))); //Sign with the new certificate, so requestor can check that it is correctly deployed
}

messaging::MessageBatches SigningServer::handleCertificateReplacementCommitRequest(std::shared_ptr<SignedCertificateReplacementCommitRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, signedRequest->getLeafCertificateOrganizationalUnit(), "Committing renewed certificates");


  if (request.getCertificateChain() != mIdentityFiles->identity()->getCertificateChain()) {
    throw Error("Cannot commit replaced certificate for server, since the certificate chain in the request does not match the current certificate chain of the server");
  }

  if (!mNewPrivateKey) {
    throw Error("Cannot commit replaced certificate for server, since the server does not have a new private key.");
  }

  if (mIdentityFiles->identity()->getPrivateKey() != *mNewPrivateKey) {
    throw Error("Cannot commit the certificate and private key that are currently in use, because the current private key is different from the new private key.");
  }

  if (!mIdentityFiles->identity()->getCertificateChain().certifiesPrivateKey(*mNewPrivateKey)) {
    throw Error("Cannot commit replaced certificate for server, since the certificate does not match the new private key of the server.");
  }

  if (!mIdentityFiles->identity()->getCertificateChain().verify(*getRootCAs())) {
    throw Error("Cannot commit replaced certificate for server, since the new certificate chain cannot be verified.");
  }

  WriteFile(mIdentityFiles->getPrivateKeyFilePath(), mIdentityFiles->identity()->getPrivateKey().toPem());
  WriteFile(mIdentityFiles->getCertificateChainFilePath(), X509CertificatesToPem(mIdentityFiles->identity()->getCertificateChain().certificates()));

  return messaging::BatchSingleMessage(Serialization::ToString(CertificateReplacementCommitResponse()));
}

SigningServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(io_context, config), mIdentityFiles(MakeSharedCopy(X509IdentityFiles::FromConfig(config, "PEP"))) {
}

}
