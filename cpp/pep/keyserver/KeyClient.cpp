#include <pep/async/RxUtils.hpp>
#include <pep/keyserver/KeyClient.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/messaging/MessagingSerializers.hpp>

namespace pep {

rxcpp::observable<PingResponse> KeyClient::requestPing() const {
  return this->ping<PingResponse>([](PingResponse response) { return response; });
}

rxcpp::observable<EnrollmentResponse> KeyClient::requestUserEnrollment(EnrollmentRequest request) const {
  return this->sendRequest<EnrollmentResponse>(std::move(request))
    .op(RxGetOne("EnrollmentResponse"));
}

rxcpp::observable<TokenBlockingListResponse> KeyClient::requestTokenBlockingList() const {
  return this->sendRequest<TokenBlockingListResponse>(this->sign(TokenBlockingListRequest()))
    .op(RxGetOne("TokenBlockingListResponse"));
}

rxcpp::observable<TokenBlockingCreateResponse> KeyClient::requestTokenBlockingCreate(TokenBlockingCreateRequest request) const {
  return this->sendRequest<TokenBlockingCreateResponse>(this->sign(std::move(request)))
    .op(RxGetOne("TokenBlockingCreateResponse"));
}

rxcpp::observable<TokenBlockingRemoveResponse> KeyClient::requestTokenBlockingRemove(TokenBlockingRemoveRequest request) const {
  return this->sendRequest<TokenBlockingRemoveResponse>(this->sign(std::move(request)))
    .op(RxGetOne("TokenBlockingRemoveResponse"));
}

}
