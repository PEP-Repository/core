#pragma once

#include <pep/authserver/AuthserverMessages.hpp>
#include <pep/server/SigningServerClient.hpp>

namespace pep {

class AuthClient : public SigningServerClient {
public:
  using SigningServerClient::SigningServerClient;

  rxcpp::observable<TokenResponse> requestToken(TokenRequest request) const;
};

}
