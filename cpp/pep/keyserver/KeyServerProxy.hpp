#pragma once

#include <pep/keyserver/KeyServerMessages.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/ServerProxy.hpp>

namespace pep {

class KeyServerProxy : public ServerProxy {
public:
  using ServerProxy::ServerProxy;

  rxcpp::observable<PingResponse> requestPing() const;
  rxcpp::observable<EnrollmentResponse> requestUserEnrollment(EnrollmentRequest request) const;

  rxcpp::observable<TokenBlockingListResponse> requestTokenBlockingList() const;
  rxcpp::observable<TokenBlockingCreateResponse> requestTokenBlockingCreate(TokenBlockingCreateRequest request) const;
  rxcpp::observable<TokenBlockingRemoveResponse> requestTokenBlockingRemove(TokenBlockingRemoveRequest request) const;
};

}
