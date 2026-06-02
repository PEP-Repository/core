#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>

namespace pep {

enum class EnrollmentScheme {
  // deprecated. Uses the protobuf serialization of the certificate of the user to derive keys.
  // This is not guaranteed to be stable. See issue #567
  V1,
  // Uses DER serialization of the certificate for key component derivation, which is guaranteed to be stable.
  V2,
  Current = V2
};

struct EnrolledPartyKeys {
  std::optional<ElgamalPrivateKey> pseudonymKey;
  std::optional<ElgamalPrivateKey> dataKey;
  // Null for services (non-users)
  std::optional<X509Identity> signingIdentity;
};

}
