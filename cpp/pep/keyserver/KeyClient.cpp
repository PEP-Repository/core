#include <pep/keyserver/KeyClient.hpp>
#include <pep/keyserver/KeyServerSerializers.hpp>
#include <pep/messaging/MessagingSerializers.hpp>

namespace pep {

rxcpp::observable<PingResponse> KeyClient::requestPing() const {
  return this->ping<PingResponse>([](PingResponse response) { return response; });
}

rxcpp::observable<EnrollmentResponse> KeyClient::requestUserEnrollment(EnrollmentRequest request) const {
  return this->requestSingleResponse<EnrollmentResponse>(std::move(request));
}

rxcpp::observable<TokenBlockingListResponse> KeyClient::requestTokenBlockingList() const {
  return this->requestSingleResponse<TokenBlockingListResponse>(this->sign(TokenBlockingListRequest()));
}

rxcpp::observable<TokenBlockingCreateResponse> KeyClient::requestTokenBlockingCreate(TokenBlockingCreateRequest request) const {
  return this->requestSingleResponse<TokenBlockingCreateResponse>(this->sign(std::move(request)));
}

rxcpp::observable<TokenBlockingRemoveResponse> KeyClient::requestTokenBlockingRemove(TokenBlockingRemoveRequest request) const {
  return this->requestSingleResponse<TokenBlockingRemoveResponse>(this->sign(std::move(request)));
}

}
