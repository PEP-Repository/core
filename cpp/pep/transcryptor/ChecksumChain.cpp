#include <pep/utils/Log.hpp>
#include <pep/transcryptor/ChecksumChain.hpp>
#include <cassert>
#include <stdexcept>

namespace pep::transcryptor {

ChecksumChain::Checkpoint ChecksumChain::SeqNoToCheckpoint(int64_t seqNo) noexcept {
  return FirstRecordCheckpoint + static_cast<uint64_t>(seqNo);
}

int64_t ChecksumChain::CheckpointToSeqNo(Checkpoint checkpoint) noexcept {
  assert(checkpoint >= FirstRecordCheckpoint);
  return static_cast<int64_t>(checkpoint - FirstRecordCheckpoint);
}

ChecksumChain::Result ChecksumChain::get(Checkpoint maxCheckpoint) {
  if (maxCheckpoint < EmptyTableCheckpoint) {
    throw std::runtime_error("Invalid checkpoint " + std::to_string(maxCheckpoint));
  }

  if (maxCheckpoint < lastResult_.checkpoint) {
    PEP_LOG("Transcryptor checksum chains", Severity::Info) << "Discarding pre-calculated checksum for checkpoint " << lastResult_.checkpoint
      << " for chain " << name_
      << " because earlier checkpoint " << maxCheckpoint << " has been requested";
    lastResult_ = InitialResult;
  }
  if (maxCheckpoint == lastResult_.checkpoint) {
    return lastResult_;
  }

  assert(maxCheckpoint >= FirstRecordCheckpoint);
  return lastResult_ = this->recalculate(lastResult_, maxCheckpoint);
}

}
