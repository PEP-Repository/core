#include <pep/accessmanager/ChecksumChain.hpp>

namespace pep::accessmanager {

int64_t ChecksumChain::CheckpointToSeqNo(Checkpoint checkpoint) noexcept {
  return static_cast<int64_t>(checkpoint - 2U);
}

ChecksumChain::ChecksumChain(std::string name, AccessManager::Backend::Storage::Implementor& storage) noexcept
  : pep::ChecksumChain(std::move(name)), storage_(storage) {
}

ChecksumChain::Checkpoint ChecksumChain::SeqNoToCheckpoint(int64_t seqno) noexcept {
  return static_cast<Checkpoint>(seqno + 2);
}

int64_t ChecksumChain::GetMaxSeqNo(const std::optional<Checkpoint>& maxCheckpoint) {
  return CheckpointToSeqNo(maxCheckpoint.value_or(std::numeric_limits<int64_t>::max()));
}

}
