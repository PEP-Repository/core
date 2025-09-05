#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServer.hpp>

namespace pep {

SigningServer::SigningServer(std::shared_ptr<Parameters> parameters)
  : Server(parameters), mPrivateKey(parameters->getPrivateKey()), mCertificateChain(parameters->getCertificateChain()) {
}

std::string SigningServer::makeSerializedPingResponse(const PingRequest& request) const {
  return Serialization::ToString(SignedPingResponse(PingResponse(request.mId), mCertificateChain, mPrivateKey));
}

}
