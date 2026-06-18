#pragma once

#include <pep/auth/Signed.hpp>


namespace pep {
struct TokenRequest {
  std::string subject_;
  std::string group_;
  Timestamp expirationTime_;
};

class TokenResponse {
public:
  TokenResponse() = default;
  TokenResponse(std::string token) : token_(std::move(token)) { }
  std::string token_;
};

using SignedTokenRequest = Signed<TokenRequest>;

}
