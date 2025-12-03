#pragma once

#include <pep/crypto/Signed.hpp>

namespace pep {

struct CsrRequest {
};
using SignedCsrRequest = Signed<CsrRequest>;

struct CsrResponse {
  X509CertificateSigningRequest mCsr;
};
using SignedCsrResponse = Signed<CsrResponse>;

struct CertificateReplacementRequest {
  X509CertificateChain mCertificateChain;
  bool mForce;
};
using SignedCertificateReplacementRequest = Signed<CertificateReplacementRequest>;

struct CertificateReplacementResponse {
};
using SignedCertificateReplacementResponse = Signed<CertificateReplacementResponse>;

struct CertificateReplacementCommitRequest {
  X509CertificateChain mCertificateChain;
};
using SignedCertificateReplacementCommitRequest = Signed<CertificateReplacementCommitRequest>;

struct CertificateReplacementCommitResponse {
};


}