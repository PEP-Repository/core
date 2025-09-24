#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace pep {

class KeyFactorSecret {
  std::array<std::byte, 64> hmacKey_;
public:
  KeyFactorSecret(std::span<const std::byte, 64> key);

  [[nodiscard]] const auto& hmacKey() const { return hmacKey_; }
};

class MasterPrivateKeyShare {
  std::array<std::byte, 32> curveScalar_;
public:
  MasterPrivateKeyShare(std::span<const std::byte, 32> key);

  [[nodiscard]] const auto& curveScalar() const { return curveScalar_; }
};

} // namespace pep
