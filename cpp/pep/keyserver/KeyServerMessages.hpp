#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/keyserver/tokenblocking/BlocklistEntry.hpp>
#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>

namespace pep {

struct EnrollmentRequest {
  X509CertificateSigningRequest certificateSigningRequest;
  std::string oAuthToken;
};

struct EnrollmentResponse {
  X509CertificateChain certificateChain;
};

struct TokenBlockingListRequest final {};

using SignedTokenBlockingListRequest = Signed<TokenBlockingListRequest>;

struct TokenBlockingListResponse final {
  std::vector<tokenBlocking::BlocklistEntry> entries;
};

struct TokenBlockingCreateRequest final {
  tokenBlocking::TokenIdentifier target;
  std::string note;
  std::optional<Timestamp> blockStartDateTime{};
};

using SignedTokenBlockingCreateRequest = Signed<TokenBlockingCreateRequest>;

struct TokenBlockingCreateResponse final {
  tokenBlocking::BlocklistEntry entry;
};

struct TokenBlockingRemoveRequest final {
  int64_t id;
};

using SignedTokenBlockingRemoveRequest = Signed<TokenBlockingRemoveRequest>;

struct TokenBlockingRemoveResponse final {
  tokenBlocking::BlocklistEntry entry;
};

} // namespace pep
