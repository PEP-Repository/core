#include <pep/networking/NetworkingSerializers.hpp>
#include <pep/networking/TLSMessageServer.hpp>

namespace pep {

TLSMessageServer::TLSMessageServer(std::shared_ptr<Parameters> parameters) :
  TLSServer<TLSMessageProtocol>(parameters),
  mRootCAs(parameters->getRootCAs()),
  mRootCAFilePath(parameters->getRootCACertificatesFilePath()) {
}

std::filesystem::path pep::TLSMessageServer::ensureDirectoryPath(std::filesystem::path path) {
  return path / "";
}

std::optional<std::filesystem::path> pep::TLSMessageServer::getStoragePath() {
  return std::nullopt;
}

TLSSignedMessageServer::TLSSignedMessageServer(std::shared_ptr<Parameters> parameters)
  : TLSMessageServer(parameters), mPrivateKey(parameters->getPrivateKey()), mCertificateChain(parameters->getCertificateChain()) {
  registerRequestHandlers(this, &TLSSignedMessageServer::handlePingRequest);
}

rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> TLSSignedMessageServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  PingResponse resp(request->mId);
  rxcpp::observable<std::shared_ptr<std::string>> retval = rxcpp::observable<>::from(
          std::make_shared<std::string>(Serialization::ToString(
    SignedPingResponse(resp, mCertificateChain, mPrivateKey))));
  return rxcpp::observable<>::from(retval);
}

}
