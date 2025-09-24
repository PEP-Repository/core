#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <boost/noncopyable.hpp>

namespace pep {

struct TranscryptorStorageBackend;

namespace transcryptor {

/*!
 * \brief Base class for checksum chain calculations (which derived classes implement in their their "calculate" methods).
 * \remark Caches the last calculated result, allowing it to be
 *         - produced from memory instead of recalculated when re-requested for the same checkpoint (as PEP's Watchdog is prone to do).
 *         - used as a partial result for the calculation of checksums at later checkpoints, eliminating more (needless re-)calculations.
 */
class ChecksumChain : boost::noncopyable {
protected:
  static constexpr uint64_t EMPTY_TABLE_CHECKPOINT = 1;
  static constexpr uint64_t FIRST_RECORD_CHECKPOINT = EMPTY_TABLE_CHECKPOINT + 1U; // Record sequence number 0 (zero) is checkpoint 2

public:
  struct Result {
    uint64_t checksum = 0;
    uint64_t checkpoint = EMPTY_TABLE_CHECKPOINT;
  };

private:
  std::string mName;
  Result mLastResult;

protected:
  explicit ChecksumChain(std::string name) noexcept : mName(std::move(name)) {}

  virtual Result calculate(std::shared_ptr<TranscryptorStorageBackend> storage, const Result& partial, uint64_t maxCheckpoint) const = 0;

  static uint64_t SeqNoToCheckpoint(int64_t seqNo) noexcept;
  static int64_t CheckpointToSeqNo(uint64_t checkpoint) noexcept;

public:
  virtual ~ChecksumChain() noexcept = default;

  std::string name() const noexcept { return mName; }

  /*!
   * \brief Returns the checksum chain's value at the highest available checkpoint not exceeding the specified one
   * \remark Caches the last computed result to prevent excessive (re-)calculation.
   */
  Result get(std::shared_ptr<TranscryptorStorageBackend> storage, uint64_t maxCheckpoint);
};


} // End namespace transcryptor
} // End namespace pep
