#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/CurveScalar.hpp>

#include <optional>

namespace pep {

struct KeyComponentRequest {
};

using SignedKeyComponentRequest = Signed<KeyComponentRequest>;

struct KeyComponentResponse {
  CurveScalar pseudonymEncryptionKeyComponent;
  std::optional<CurveScalar> dataEncryptionKeyComponent;
};

}
