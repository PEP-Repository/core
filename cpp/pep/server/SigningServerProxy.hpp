#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/ServerProxy.hpp>

namespace pep {

class SigningServerProxy : public ServerProxy {
public:
  using ServerProxy::ServerProxy;

  rxcpp::observable<SignedPingResponse> requestPing() const;
};

}
