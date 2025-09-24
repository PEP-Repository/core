#pragma once

#include <pep/rsk/RskTranslator.hpp>

namespace pep {

enum class KeyDomain : RskTranslator::KeyDomainType {
  /// Encrypted pseudonyms
  Pseudonym = 1,
  /// Encrypted data
  Data = 2,
};

} // namespace pep
