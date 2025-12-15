#include <pep/auth/Certificate.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), MessageSigner(parameters->getSigningIdentity()), mIdentityFilesConfig(parameters->getIdentityFilesConfig()) {
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
  CsrResponse response;
  response.mCsr = X509CertificateSigningRequest::CreateWithSubjectFromExistingCertificate(newKeyPair, mIdentityFilesConfig->identity()->getCertificateChain().front());
  return messaging::BatchSingleMessage(Serialization::ToString(sign(response)));
}

messaging::MessageBatches SigningServer::handleCertificateReplacementRequest(std::shared_ptr<SignedCertificateReplacementRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, signedRequest->getLeafCertificateOrganizationalUnit(), "Renewing certificates");

  if (!mNewPrivateKey) {
    throw Error("Cannot replace certificate for server, since the server does not have a new private key.");
  }

  if (!request.mCertificateChain.certifiesPrivateKey(*mNewPrivateKey)) {
    throw Error("Cannot replace certificate for server, since the certificate does not match the new private key of the server.");
  }

  if (!request.mForce && !request.mCertificateChain.front().hasSameSubject(mIdentityFilesConfig->identity()->getCertificateChain().front())) {
    throw Error("New certificate has a different subject from the current certificate. Use --force to force replacing the certificate.");
  }

  if (!IsServerSigningCertificate(mIdentityFilesConfig->identity()->getCertificateChain().front())) {
    throw std::runtime_error("New certificate is not a server signing certificate");
  }

  if (!request.mCertificateChain.verify(*getRootCAs())) {
    throw Error("Cannot replace certificate for server, since the new certificate chain cannot be verified.");
  }

  *mIdentityFilesConfig->identity() = X509Identity(*mNewPrivateKey, request.mCertificateChain);

  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(CertificateReplacementResponse()))); //Sign with the new certificate, so requestor can check that it is correctly deployed
}

messaging::MessageBatches SigningServer::handleCertificateReplacementCommitRequest(std::shared_ptr<SignedCertificateReplacementCommitRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  UserGroup::EnsureAccess({UserGroup::AccessAdministrator}, signedRequest->getLeafCertificateOrganizationalUnit(), "Committing renewed certificates");


  if (request.mCertificateChain != mIdentityFilesConfig->identity()->getCertificateChain()) {
    throw Error("Cannot commit replaced certificate for server, since the certificate chain in the request does not match the current certificate chain of the server");
  }

  if (!mNewPrivateKey) {
    throw Error("Cannot commit replaced certificate for server, since the server does not have a new private key.");
  }

  if (mIdentityFilesConfig->identity()->getPrivateKey() != *mNewPrivateKey) {
    throw Error("Cannot commit the certificate and private key that are currently in use, because the current private key is different from the new private key.");
  }

  if (!mIdentityFilesConfig->identity()->getCertificateChain().certifiesPrivateKey(*mNewPrivateKey)) {
    throw Error("Cannot commit replaced certificate for server, since the certificate does not match the new private key of the server.");
  }

  if (!mIdentityFilesConfig->identity()->getCertificateChain().verify(*getRootCAs())) {
    throw Error("Cannot commit replaced certificate for server, since the new certificate chain cannot be verified.");
  }

  WriteFile(mIdentityFilesConfig->getPrivateKeyFilePath(), mIdentityFilesConfig->identity()->getPrivateKey().toPem());
  WriteFile(mIdentityFilesConfig->getCertificateChainFilePath(), mIdentityFilesConfig->identity()->getCertificateChain().toPem());

  return messaging::BatchSingleMessage(Serialization::ToString(CertificateReplacementCommitResponse()));
}

SigningServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(io_context, config), mIdentityFilesConfig(std::make_shared<X509IdentityFilesConfiguration>(config, "PEP")) {
}

}
