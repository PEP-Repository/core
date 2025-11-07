#pragma once

#include <pep/crypto/Signed.hpp>


namespace pep {
struct TokenRequest {
  std::string mSubject;
  std::string mGroup;
  Timestamp mExpirationTime;
};

class TokenResponse {
public:
  TokenResponse() = default;
  TokenResponse(std::string token) : mToken(std::move(token)) { }
  std::string mToken;
};

using SignedTokenRequest = Signed<TokenRequest>;

}
