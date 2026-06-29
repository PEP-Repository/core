#include <pep/utils/Log.hpp>
#include <pep/transcryptor/ChecksumChain.hpp>
#include <cassert>
#include <stdexcept>

namespace pep::transcryptor {

uint64_t ChecksumChain::SeqNoToCheckpoint(int64_t seqNo) noexcept {
  return FirstRecordCheckpoint + static_cast<uint64_t>(seqNo);
}

int64_t ChecksumChain::CheckpointToSeqNo(uint64_t checkpoint) noexcept {
  assert(checkpoint >= FirstRecordCheckpoint);
  return static_cast<int64_t>(checkpoint - FirstRecordCheckpoint);
}

ChecksumChain::Result ChecksumChain::get(std::shared_ptr<TranscryptorStorageBackend> storage, uint64_t maxCheckpoint) {
  if (maxCheckpoint < EmptyTableCheckpoint) {
    throw std::runtime_error("Invalid checkpoint " + std::to_string(maxCheckpoint));
  }

  if (maxCheckpoint < lastResult_.checkpoint) {
    PEP_LOG("Transcryptor checksum chains", Severity::Info) << "Discarding pre-calculated checksum for checkpoint " << lastResult_.checkpoint
      << " for chain " << name_
      << " because earlier checkpoint " << maxCheckpoint << " has been requested";
    lastResult_ = Result();
  }
  if (maxCheckpoint == lastResult_.checkpoint) {
    return lastResult_;
  }

  assert(maxCheckpoint >= FirstRecordCheckpoint);
  return lastResult_ = this->calculate(storage, lastResult_, maxCheckpoint);
}

}
