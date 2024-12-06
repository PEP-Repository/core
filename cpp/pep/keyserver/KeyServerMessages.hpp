#pragma once

#include <pep/crypto/Signed.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/keyserver/tokenblocking/BlocklistEntry.hpp>
#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>

namespace pep {

class EnrollmentRequest {
public:
  inline EnrollmentRequest() {}
  inline EnrollmentRequest(
    const X509CertificateSigningRequest csr,
    std::string oauthToken
  ) :
    mOAuthToken(std::move(oauthToken)),
    mCertificateSigningRequest(csr) {
  }

  std::string mOAuthToken;
  X509CertificateSigningRequest mCertificateSigningRequest;
};

class EnrollmentResponse {
public:
  inline EnrollmentResponse() {}

  X509CertificateChain mCertificateChain;
};

using TokenBlockingTokenIdentifier = tokenBlocking::TokenIdentifier;
using TokenBlockingBlocklistEntry = tokenBlocking::BlocklistEntry;

struct TokenBlockingListRequest final {};

using SignedTokenBlockingListRequest = Signed<TokenBlockingListRequest>;

struct TokenBlockingListResponse final {
  std::vector<TokenBlockingBlocklistEntry> entries;
};

struct TokenBlockingCreateRequest final {
  TokenBlockingTokenIdentifier target;
  std::string note;
};

using SignedTokenBlockingCreateRequest = Signed<TokenBlockingCreateRequest>;

struct TokenBlockingCreateResponse final {
  TokenBlockingBlocklistEntry entry;
};

struct TokenBlockingRemoveRequest final {
  int64_t id;
};

using SignedTokenBlockingRemoveRequest = Signed<TokenBlockingRemoveRequest>;

struct TokenBlockingRemoveResponse final {
  TokenBlockingBlocklistEntry entry;
};

} // namespace pep
