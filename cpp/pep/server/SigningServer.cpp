#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/utils/Shared.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), mSigningIdentity(parameters->getSigningIdentity()) {
}

std::string SigningServer::makeSerializedPingResponse(const PingRequest& request) const {
  return Serialization::ToString(SignedPingResponse(PingResponse(request.mId), *mSigningIdentity));
}

SigningServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : Server::Parameters(io_context, config), identity_(MakeSharedCopy(X509IdentityFilesConfiguration(config, "PEP").identity())) {
}

}
