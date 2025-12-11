#include <pep/async/RxRequireCount.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

rxcpp::observable<SignedPingResponse> SigningServerProxy::requestSignedPing(const PingRequest& request) const {
  return this->sendRequest<SignedPingResponse>(request)
    .op(RxGetOne());
}

rxcpp::observable<PingResponse> SigningServerProxy::requestPing() const {
  return this->requestSignedPing(PingRequest{})
    .map([](const SignedPingResponse& response) { return response.openWithoutCheckingSignature(); });
}

rxcpp::observable<X509CertificateChain> SigningServerProxy::requestCertificateChain() const {
  PingRequest request;
  return this->requestSignedPing(request)
    .map([request](const SignedPingResponse& response) {
    response.openWithoutCheckingSignature().validate(request);
    return response.mSignature.mCertificateChain;
      });

}

}
