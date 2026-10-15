#pragma once

#include <pep/transcryptor/Storage.hpp>
#include <boost/noncopyable.hpp>

namespace pep {

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

  TranscryptorStorage::Implementor& storage_;
  mutable Result lastResult_ = InitialResult;

protected:
  ChecksumChain(TranscryptorStorage::Implementor& storage, std::string name) noexcept : pep::ChecksumChain(std::move(name)), storage_(storage) {}

  TranscryptorStorage::Implementor& storage() const noexcept { return storage_; }

  virtual Result recalculate(const Result& partial, Checkpoint maxCheckpoint) const = 0;

  static Checkpoint SeqNoToCheckpoint(int64_t seqNo) noexcept;
  static int64_t CheckpointToSeqNo(Checkpoint checkpoint) noexcept;

public:
  /// \remark Caches the last computed result to prevent excessive (re-)calculation.
  Result get(const std::optional<Checkpoint>& maxCheckpoint) const override;
};


} // End namespace transcryptor
} // End namespace pep
