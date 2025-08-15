#pragma once

#include <pep/rsk/RskKeys.hpp>

namespace pep {

struct PseudonymTranslationKeys {
  /// Pseudonym encryption key factor secret (rekey)
  KeyFactorSecret encryptionKeyFactorSecret;
  /// Pseudonymization key factor secret (reshuffle)
  KeyFactorSecret pseudonymizationKeyFactorSecret;
  /// Master private pseudonym encryption key share
  MasterPrivateKeyShare masterPrivateEncryptionKeyShare;
};

} // namespace pep
