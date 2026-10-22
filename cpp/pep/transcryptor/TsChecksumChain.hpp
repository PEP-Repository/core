#pragma once

#include <pep/transcryptor/Storage.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <boost/noncopyable.hpp>

namespace pep {

class TsChecksumChain : boost::noncopyable, public ChecksumChain {
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
  TsChecksumChain(TranscryptorStorage::Implementor& storage, std::string name) noexcept;

  TranscryptorStorage::Implementor& storage() const noexcept { return storage_; }

  virtual Result recalculate(const Result& partial, Checkpoint maxCheckpoint) const = 0;

  static Checkpoint SeqNoToCheckpoint(int64_t seqNo) noexcept;
  static int64_t CheckpointToSeqNo(Checkpoint checkpoint) noexcept;

public:
  /// \remark Caches the last computed result to prevent excessive (re-)calculation.
  Result get(const std::optional<Checkpoint>& maxCheckpoint) const override;
};

}
