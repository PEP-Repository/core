#pragma once

#include <pep/auth/Signed.hpp>

namespace pep {

struct CsrRequest {};

struct CsrResponse {
  X509CertificateSigningRequest csr;
};

struct CertificateReplacementRequest {
  X509CertificateChain certificateChain;
  bool allowChangingSubject;
};

struct CertificateReplacementResponse {};

struct CertificateReplacementCommitRequest {
  X509CertificateChain certificateChain;
};

struct CertificateReplacementCommitResponse {};

using SignedCsrRequest = Signed<CsrRequest>;
using SignedCsrResponse = Signed<CsrResponse>;
using SignedCertificateReplacementRequest = Signed<CertificateReplacementRequest>;
using SignedCertificateReplacementResponse = Signed<CertificateReplacementResponse>;
using SignedCertificateReplacementCommitRequest = Signed<CertificateReplacementCommitRequest>;

}
