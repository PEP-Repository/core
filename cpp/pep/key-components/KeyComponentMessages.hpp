#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/CurveScalar.hpp>

namespace pep {

class KeyComponentRequest {
};

using SignedKeyComponentRequest = Signed<KeyComponentRequest>;

class KeyComponentResponse {
public:
  KeyComponentResponse() = default;
  inline KeyComponentResponse(
    const CurveScalar& pseudonymKeyComponent,
    const CurveScalar& encryptionKeyComponent
  ) : mPseudonymEncryptionKeyComponent(pseudonymKeyComponent),
    dataEncryptionKeyComponent_(encryptionKeyComponent) {}

  CurveScalar mPseudonymEncryptionKeyComponent;
  CurveScalar dataEncryptionKeyComponent_;
};

}
