#pragma once

#include <pep/crypto/Signed.hpp>

namespace pep {

struct CsrRequest {
};
using SignedCsrRequest = Signed<CsrRequest>;

struct CsrResponse {
  X509CertificateSigningRequest mCsr;
};

struct CertificateReplacementRequest {
  X509CertificateChain mCertificateChain;
  bool mForce;
};
using SignedCertificateReplacementRequest = Signed<CertificateReplacementRequest>;

struct CertificateReplacementResponse {
};
using SignedCertificateReplacementResponse = Signed<CertificateReplacementResponse>;

struct CertificateReplacementCommitRequest {
};
using SignedCertificateReplacementCommitRequest = Signed<CertificateReplacementCommitRequest>;

struct CertificateReplacementCommitResponse {
};


}