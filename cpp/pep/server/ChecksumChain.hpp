#pragma once

#include <pep/utils/PropertyBasedContainer.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace pep {

class ChecksumChain {
private:
  std::string name_;

protected:
  ChecksumChain(std::string name) noexcept : name_(std::move(name)) {}

public:
  using Checksum = uint64_t;
  using Checkpoint = uint64_t;

  struct Result {
    Checksum checksum = 0;
    Checkpoint checkpoint = 0;
  };

  virtual ~ChecksumChain() noexcept = default;

  const std::string& name() const noexcept { return name_; }

  /// \brief Returns the checksum chain's value at the highest available checkpoint not exceeding the specified one.
  /// \param maxCheckpoint The maximum checkpoint value to include in the checksum, or nullopt to include all available checkpoints.
  /// \return The result of the checksum (calculation).
  virtual Result get(const std::optional<Checkpoint>& maxCheckpoint) = 0;
};


class ChecksumChains {
private:
  PropertyBasedContainer<std::unique_ptr<ChecksumChain>, &ChecksumChain::name>::set items_;

  void insert(std::unique_ptr<ChecksumChain> chain);

public:
  template <typename... TUniquePtrs>
  explicit ChecksumChains(TUniquePtrs... ptrs) {
    (this->insert(std::move(ptrs)), ...);
  }

  std::set<std::string> names() const;
  ChecksumChain& operator[](const std::string& name);
};

}
