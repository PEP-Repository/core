#include <pep/utils/Log.hpp>
#include <pep/transcryptor/TsChecksumChain.hpp>
#include <cassert>
#include <stdexcept>

namespace pep {

TsChecksumChain::Checkpoint TsChecksumChain::SeqNoToCheckpoint(int64_t seqNo) noexcept {
  return FirstRecordCheckpoint + static_cast<uint64_t>(seqNo);
}

int64_t TsChecksumChain::CheckpointToSeqNo(Checkpoint checkpoint) noexcept {
  assert(checkpoint >= FirstRecordCheckpoint);
  return static_cast<int64_t>(checkpoint - FirstRecordCheckpoint);
}

TsChecksumChain::Result TsChecksumChain::get(const std::optional<Checkpoint>& maxCheckpoint) const {
  auto max = maxCheckpoint.value_or(std::numeric_limits<int64_t>::max());

  if (max < EmptyTableCheckpoint) {
    throw std::runtime_error("Invalid checkpoint " + std::to_string(max));
  }

  if (max < lastResult_.checkpoint) {
    PEP_LOG("Transcryptor checksum chains", Severity::Info) << "Discarding pre-calculated checksum for checkpoint " << lastResult_.checkpoint
      << " for chain " << this->name()
      << " because earlier checkpoint " << max << " has been requested";
    lastResult_ = InitialResult;
  }
  if (max == lastResult_.checkpoint) {
    return lastResult_;
  }

  assert(max >= FirstRecordCheckpoint);
  return lastResult_ = this->recalculate(lastResult_, max);
}

}
