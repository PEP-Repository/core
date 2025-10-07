#include <pep/async/RxUtils.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServerClient.hpp>

namespace pep {

rxcpp::observable<SignedPingResponse> SigningServerClient::requestPing() const {
  PingRequest request;
  return this->sendRequest<SignedPingResponse>(request)
    .op(RxGetOne("SignedPingResponse"))
    .tap([request](const SignedPingResponse& response) { response.openWithoutCheckingSignature().validate(request); });
}

}
