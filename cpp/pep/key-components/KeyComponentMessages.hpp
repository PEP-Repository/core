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
  ) : pseudonymEncryptionKeyComponent_(pseudonymKeyComponent),
    dataEncryptionKeyComponent_(encryptionKeyComponent) {}

  CurveScalar pseudonymEncryptionKeyComponent_;
  CurveScalar dataEncryptionKeyComponent_;
};

}
