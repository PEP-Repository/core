#pragma once

#include <pep/crypto/Signed.hpp>


namespace pep {
class TokenRequest {
public:
  TokenRequest() = default;
  TokenRequest(std::string subject, std::string group, Timestamp expirationTime)
    : mSubject(std::move(subject)), mGroup(std::move(group)), mExpirationTime(expirationTime) { }
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
