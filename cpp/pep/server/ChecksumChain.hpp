#pragma once

#include <cstdint>
#include <optional>

namespace pep {

struct ChecksumChain {
  using Checksum = uint64_t;
  using Checkpoint = uint64_t;

  struct Result {
    Checksum checksum = 0;
    Checkpoint checkpoint = 0;
  };

  virtual ~ChecksumChain() noexcept = default;

  /// \brief Returns the checksum chain's value at the highest available checkpoint not exceeding the specified one.
  /// \param maxCheckpoint The maximum checkpoint value to include in the checksum, or nullopt to include all available checkpoints.
  /// \return The result of the checksum (calculation).
  virtual Result get(const std::optional<Checkpoint>& maxCheckpoint) = 0;
};

}
