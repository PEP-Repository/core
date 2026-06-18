#pragma once

#include <pep/auth/Signed.hpp>

namespace pep {

class CsrRequest {
};
using SignedCsrRequest = Signed<CsrRequest>;

class CsrResponse {
  X509CertificateSigningRequest csr_;

public:
  CsrResponse(X509CertificateSigningRequest csr) : csr_(std::move(csr)) {}
  const X509CertificateSigningRequest& getCsr() const { return csr_; }
};
using SignedCsrResponse = Signed<CsrResponse>;

class CertificateReplacementRequest {
  X509CertificateChain certificateChain_;
  bool allowChangingSubject_;

public:
  CertificateReplacementRequest(X509CertificateChain chain, bool allowChangingSubject) : certificateChain_(std::move(chain)), allowChangingSubject_(allowChangingSubject) {}
  const X509CertificateChain& getCertificateChain() const { return certificateChain_; }
  bool allowChangingSubject() const { return allowChangingSubject_; }
};
using SignedCertificateReplacementRequest = Signed<CertificateReplacementRequest>;

class CertificateReplacementResponse {
};
using SignedCertificateReplacementResponse = Signed<CertificateReplacementResponse>;

class CertificateReplacementCommitRequest {
  X509CertificateChain certificateChain_;

public:
  CertificateReplacementCommitRequest(X509CertificateChain chain) : certificateChain_(std::move(chain)) {}
  const X509CertificateChain& getCertificateChain() const { return certificateChain_; }
};
using SignedCertificateReplacementCommitRequest = Signed<CertificateReplacementCommitRequest>;

class CertificateReplacementCommitResponse {
};


}