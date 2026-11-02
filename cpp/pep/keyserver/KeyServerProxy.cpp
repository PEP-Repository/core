#include <pep/async/RxRequireCount.hpp>
#include <pep/keyserver/KeyServerProxy.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/messaging/MessagingSerializers.hpp>

namespace pep {

rxcpp::observable<PingResponse> KeyServerProxy::requestPing() const {
  PingRequest request;
  return this->sendRequest<PingResponse>(request)
    .op(RxGetOne())
    .tap([request](const PingResponse& response) { response.validate(request); });
}

rxcpp::observable<EnrollmentResponse> KeyServerProxy::requestUserEnrollment(EnrollmentRequest request) const {
  return this->sendRequest<EnrollmentResponse>(std::move(request))
    .op(RxGetOne());
}

rxcpp::observable<TokenBlockingListResponse> KeyServerProxy::requestTokenBlockingList() const {
  return this->sendRequest<TokenBlockingListResponse>(this->sign(TokenBlockingListRequest()))
    .op(RxGetOne());
}

rxcpp::observable<TokenBlockingCreateResponse> KeyServerProxy::requestTokenBlockingCreate(TokenBlockingCreateRequest request) const {
  return this->sendRequest<TokenBlockingCreateResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<TokenBlockingRemoveResponse> KeyServerProxy::requestTokenBlockingRemove(TokenBlockingRemoveRequest request) const {
  return this->sendRequest<TokenBlockingRemoveResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

}
