#include <pep/utils/Log.hpp>
#include <pep/transcryptor/ChecksumChain.hpp>
#include <cassert>
#include <stdexcept>

namespace pep::transcryptor {

uint64_t ChecksumChain::SeqNoToCheckpoint(int64_t seqNo) noexcept {
  return FIRST_RECORD_CHECKPOINT + static_cast<uint64_t>(seqNo);
}

int64_t ChecksumChain::CheckpointToSeqNo(uint64_t checkpoint) noexcept {
  assert(checkpoint >= FIRST_RECORD_CHECKPOINT);
  return static_cast<int64_t>(checkpoint - FIRST_RECORD_CHECKPOINT);
}

ChecksumChain::Result ChecksumChain::get(std::shared_ptr<TranscryptorStorageBackend> storage, uint64_t maxCheckpoint) {
  if (maxCheckpoint < EMPTY_TABLE_CHECKPOINT) {
    throw std::runtime_error("Invalid checkpoint " + std::to_string(maxCheckpoint));
  }

  if (maxCheckpoint < mLastResult.checkpoint) {
    LOG("Transcryptor checksum chains", info) << "Discarding pre-calculated checksum for checkpoint " << mLastResult.checkpoint
      << " for chain " << mName
      << " because earlier checkpoint " << maxCheckpoint << " has been requested";
    mLastResult = Result();
  }
  if (maxCheckpoint == mLastResult.checkpoint) {
    return mLastResult;
  }

  assert(maxCheckpoint >= FIRST_RECORD_CHECKPOINT);
  return mLastResult = this->calculate(storage, mLastResult, maxCheckpoint);
}

}
