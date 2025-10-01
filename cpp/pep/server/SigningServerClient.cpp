#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/ServerConnection.hpp>
#include <pep/server/SigningServerClient.hpp>
#include <pep/utils/Random.hpp>

namespace pep {

SigningServerClient::SigningServerClient(std::shared_ptr<messaging::ServerConnection> untyped, std::shared_ptr<const X509Identity> signingIdentity) noexcept
  : TypedClient(std::move(untyped), std::move(signingIdentity)) {
}

rxcpp::observable<SignedPingResponse> SigningServerClient::requestPing() const {
  return this->untyped()->ping<SignedPingResponse>(&SignedPingResponse::openWithoutCheckingSignature);
}

}
