#pragma once

#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/TypedClient.hpp>

namespace pep {

class SigningServerClient : public TypedClient{
protected:
  SigningServerClient(std::shared_ptr<messaging::ServerConnection> untyped, std::shared_ptr<const X509Identity> signingIdentity) noexcept;

public:
  rxcpp::observable<SignedPingResponse> requestPing() const;
};

}
