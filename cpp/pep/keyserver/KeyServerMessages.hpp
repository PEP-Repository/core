#pragma once

#include <pep/crypto/Signed.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/keyserver/tokenblocking/BlocklistEntry.hpp>
#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>

namespace pep {

class EnrollmentRequest {
public:
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

struct EnrollmentResponse {
  X509CertificateChain mCertificateChain;
};

struct TokenBlockingListRequest final {};

using SignedTokenBlockingListRequest = Signed<TokenBlockingListRequest>;

struct TokenBlockingListResponse final {
  std::vector<tokenBlocking::BlocklistEntry> entries;
};

struct TokenBlockingCreateRequest final {
  tokenBlocking::TokenIdentifier target;
  std::string note;
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
