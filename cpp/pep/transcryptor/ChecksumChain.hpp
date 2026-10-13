#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <memory>
#include <string>
#include <boost/noncopyable.hpp>

namespace pep {

struct TranscryptorStorageBackend;

namespace transcryptor {

/// \brief Base class for checksum chain calculations (which derived classes implement in their their "calculate" methods).
/// \remark Caches the last calculated result, allowing it to be
///         - produced from memory instead of recalculated when re-requested for the same checkpoint (as PEP's Watchdog is prone to do).
///         - used as a partial result for the calculation of checksums at later checkpoints, eliminating more (needless re-)calculations.
class ChecksumChain : boost::noncopyable, public pep::ChecksumChain {
protected:
  static constexpr Checkpoint EmptyTableCheckpoint = 1;

private:
  static constexpr Checkpoint FirstRecordCheckpoint = EmptyTableCheckpoint + 1U; // Record sequence number 0 (zero) is checkpoint 2

  static constexpr Result InitialResult{
    .checksum = 0,
    .checkpoint = EmptyTableCheckpoint,
  };

  TranscryptorStorageBackend& storage_;
  std::string name_;
  Result lastResult_ = InitialResult;

protected:
  explicit ChecksumChain(TranscryptorStorageBackend& storage, std::string name) noexcept : storage_(storage), name_(std::move(name)) {}

  TranscryptorStorageBackend& storage() const noexcept { return storage_; }

  virtual Result recalculate(const Result& partial, Checkpoint maxCheckpoint) const = 0;

  static Checkpoint SeqNoToCheckpoint(int64_t seqNo) noexcept;
  static int64_t CheckpointToSeqNo(Checkpoint checkpoint) noexcept;

public:
  virtual ~ChecksumChain() noexcept = default;

  const std::string& name() const noexcept { return name_; }

  /// \brief Returns the checksum chain's value at the highest available checkpoint not exceeding the specified one.
  /// \param maxCheckpoint The maximum checkpoint value to include in the checksum. If unspecified (nullopt), all available checkpoints are included.
  /// \remark Caches the last computed result to prevent excessive (re-)calculation.
  Result get(const std::optional<Checkpoint>& maxCheckpoint);
};


} // End namespace transcryptor
} // End namespace pep
