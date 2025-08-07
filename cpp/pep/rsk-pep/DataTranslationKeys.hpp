#pragma once

#include <pep/rsk/RskKeys.hpp>

#include <optional>

namespace pep {

//NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) False positive, only constructors that initialize are available
struct DataTranslationKeys {
  /// Data encryption key factor secret
  KeyFactorSecret encryptionKeyFactorSecret;
  /// Blinding key secret (only for AM)
  std::optional<KeyFactorSecret> blindingKeySecret;
  /// Master private data encryption key share
  MasterPrivateKeyShare masterPrivateEncryptionKeyShare;
};

} // namespace pep
