#include <pep/accessmanager/Storage.hpp>
#include <pep/server/ChecksumChain.hpp>

namespace pep {

class AmChecksumChain : public ChecksumChain {
private:
  AccessManager::Backend::Storage::Implementor& storage_;

  static int64_t CheckpointToSeqNo(Checkpoint checkpoint) noexcept;

protected:
  static Checkpoint SeqNoToCheckpoint(int64_t seqno) noexcept;
  static int64_t GetMaxSeqNo(const std::optional<Checkpoint>& maxCheckpoint);

  AmChecksumChain(std::string name, AccessManager::Backend::Storage::Implementor& storage) noexcept;

  AccessManager::Backend::Storage::Implementor& storage() const noexcept { return storage_; }
};

}
