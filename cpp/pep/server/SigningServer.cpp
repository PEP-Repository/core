#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/utils/Shared.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), MessageSigner(parameters->getSigningIdentity()) {
  RegisterRequestHandlers(*this, &SigningServer::handlePingRequest);
}

messaging::MessageBatches SigningServer::handlePingRequest(std::shared_ptr<PingRequest> request) {
  return messaging::BatchSingleMessage(Serialization::ToString(this->sign(PingResponse(request->mId))));
}

SigningServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(io_context, config), identity_(MakeSharedCopy(X509IdentityFiles::FromConfig(config, "PEP").identity())) {
}

}
