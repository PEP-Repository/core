#pragma once

#include <pep/crypto/Signed.hpp>

namespace pep {

class CsrRequest {
};
using SignedCsrRequest = Signed<CsrRequest>;

class CsrResponse {
  X509CertificateSigningRequest mCsr;

public:
  CsrResponse(X509CertificateSigningRequest csr) : mCsr(std::move(csr)) {}
  const X509CertificateSigningRequest& getCsr() const { return mCsr; }
};
using SignedCsrResponse = Signed<CsrResponse>;

class CertificateReplacementRequest {
  X509CertificateChain mCertificateChain;
  bool mForce;

public:
  CertificateReplacementRequest(X509CertificateChain chain, bool force) : mCertificateChain(std::move(chain)), mForce(force) {}
  const X509CertificateChain& getCertificateChain() const { return mCertificateChain; }
  bool force() const { return mForce; }
};
using SignedCertificateReplacementRequest = Signed<CertificateReplacementRequest>;

class CertificateReplacementResponse {
};
using SignedCertificateReplacementResponse = Signed<CertificateReplacementResponse>;

class CertificateReplacementCommitRequest {
  X509CertificateChain mCertificateChain;

public:
  CertificateReplacementCommitRequest(X509CertificateChain chain) : mCertificateChain(std::move(chain)) {}
  const X509CertificateChain& getCertificateChain() const { return mCertificateChain; }
};
using SignedCertificateReplacementCommitRequest = Signed<CertificateReplacementCommitRequest>;

class CertificateReplacementCommitResponse {
};


}