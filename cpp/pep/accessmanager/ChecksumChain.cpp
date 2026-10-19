#include <pep/accessmanager/ChecksumChain.hpp>

namespace pep {

int64_t AmChecksumChain::CheckpointToSeqNo(Checkpoint checkpoint) noexcept {
  return static_cast<int64_t>(checkpoint - 2U);
}

AmChecksumChain::AmChecksumChain(std::string name, AccessManager::Backend::Storage::Implementor& storage) noexcept
  : ChecksumChain(std::move(name)), storage_(storage) {
}

AmChecksumChain::Checkpoint AmChecksumChain::SeqNoToCheckpoint(int64_t seqno) noexcept {
  return static_cast<Checkpoint>(seqno + 2);
}

int64_t AmChecksumChain::GetMaxSeqNo(const std::optional<Checkpoint>& maxCheckpoint) {
  return CheckpointToSeqNo(maxCheckpoint.value_or(std::numeric_limits<int64_t>::max()));
}

}
