#pragma once

#include <pep/authserver/AuthserverMessages.hpp>
#include <pep/server/SigningServerProxy.hpp>

namespace pep {

class AuthClient : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<TokenResponse> requestToken(TokenRequest request) const;
};

}
