#include <pep/async/RxRequireCount.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

rxcpp::observable<SignedPingResponse> SigningServerProxy::requestPing() const {
  PingRequest request;
  return this->sendRequest<SignedPingResponse>(request)
    .op(RxGetOne())
    .tap([request](const SignedPingResponse& response) { response.openWithoutCheckingSignature().validate(request); });
}

}
