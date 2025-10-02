#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/TypedClient.hpp>

namespace pep {

class SigningServerClient : public TypedClient {
public:
  using TypedClient::TypedClient;

  rxcpp::observable<SignedPingResponse> requestPing() const;
};

}
