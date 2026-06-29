#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/CurveScalar.hpp>

namespace pep {

struct KeyComponentRequest {
};

using SignedKeyComponentRequest = Signed<KeyComponentRequest>;

struct KeyComponentResponse {
  CurveScalar pseudonymEncryptionKeyComponent;
  CurveScalar dataEncryptionKeyComponent;
};

}
