#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/utils/Timestamp.hpp>

#include <string>


namespace pep {
struct TokenRequest {
  std::string subject;
  std::string group;
  Timestamp expirationTime;
};

struct TokenResponse {
  std::string token;
};

using SignedTokenRequest = Signed<TokenRequest>;

}
