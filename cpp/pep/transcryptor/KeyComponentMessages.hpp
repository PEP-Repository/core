#pragma once

#include <pep/rsk-pep/DataTranslator.hpp>
#include <pep/rsk-pep/PseudonymTranslator.hpp>
#include <pep/auth/Signed.hpp>

namespace pep {

enum EnrollmentScheme {
  // deprecated. Uses the protobuf serialization of the certificate of the user to derive keys.
  // This is not guaranteed to be stable. See issue #567
  ENROLLMENT_SCHEME_V1 = 0,

  ENROLLMENT_SCHEME_V2 = 1,

  ENROLLMENT_SCHEME_CURRENT = ENROLLMENT_SCHEME_V2
};

class KeyComponentRequest {
};

using SignedKeyComponentRequest = Signed<KeyComponentRequest>;

class KeyComponentResponse {
public:
  KeyComponentResponse() = default;
  inline KeyComponentResponse(
    const CurveScalar& pseudonymKeyComponent,
    const CurveScalar& encryptionKeyComponent
  ) : mPseudonymKeyComponent(pseudonymKeyComponent),
    mEncryptionKeyComponent(encryptionKeyComponent) {}

  CurveScalar mPseudonymKeyComponent;
  CurveScalar mEncryptionKeyComponent;

  static KeyComponentResponse HandleRequest(
    const SignedKeyComponentRequest& request,
    const PseudonymTranslator& pseudonymTranslator,
    const DataTranslator& dataTranslator,
    const X509RootCertificates& rootCAs);
};

}
