#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServerClient.hpp>

namespace pep {

rxcpp::observable<SignedPingResponse> SigningServerClient::requestPing() const {
  return this->untyped()->ping<SignedPingResponse>(&SignedPingResponse::openWithoutCheckingSignature);
}

}
