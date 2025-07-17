#include <pep/networking/NetworkingSerializers.hpp>
#include <pep/networking/TLSMessageServer.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {

TLSMessageServer::TLSMessageServer(std::shared_ptr<Parameters> parameters) :
  TLSServer<TLSMessageProtocol>(parameters),
  mRootCAs(parameters->getRootCAs()) {
}

std::filesystem::path pep::TLSMessageServer::EnsureDirectoryPath(std::filesystem::path path) {
  return path / "";
}

std::optional<std::filesystem::path> pep::TLSMessageServer::getStoragePath() {
  return std::nullopt;
}

TLSMessageServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context,
  const Configuration& config)
  : TLSServer<TLSMessageProtocol>::Parameters(io_context, config),
  rootCACertificatesFilePath_(
    config.get<std::filesystem::path>("CACertificateFile")),
  rootCAs_(ReadFile(rootCACertificatesFilePath_)) {
}

TLSSignedMessageServer::TLSSignedMessageServer(std::shared_ptr<Parameters> parameters)
  : TLSMessageServer(parameters), mPrivateKey(parameters->getPrivateKey()), mCertificateChain(parameters->getCertificateChain()) {
  RegisterRequestHandlers(*this, &TLSSignedMessageServer::handlePingRequest);
}

MessageBatches TLSSignedMessageServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  PingResponse resp(request->mId);
  MessageSequence retval = rxcpp::observable<>::from(
          std::make_shared<std::string>(Serialization::ToString(
    SignedPingResponse(resp, mCertificateChain, mPrivateKey))));
  return rxcpp::observable<>::from(retval);
}

}
