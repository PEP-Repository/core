#include <pep/storagefacility/ChecksumChain.hpp>
#include <pep/serialization/Error.hpp>

namespace pep::storagefacility {

namespace {

class FilesChecksumChain : public ChecksumChain {
protected:
  virtual uint64_t addCellVersion(uint64_t checksum, const FileStore::CellVersion& version) const override {
    return checksum ^ version.checksumSubstitute;
  }

public:
  explicit FilesChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "files") {}
};

class EntryCountChecksumChain : public ChecksumChain {
protected:
  virtual uint64_t addCellVersion(uint64_t checksum, const FileStore::CellVersion& version) const override { return ++checksum; }

public:
  explicit EntryCountChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "entry-count") {}
};

}

using namespace std::chrono_literals;

ChecksumChain::ChecksumChain(const FileStore& fileStore, std::string name) noexcept
  : pep::ChecksumChain(std::move(name)), fileStore_(fileStore) {
}

ChecksumChain::Result ChecksumChain::get(const std::optional<Checkpoint>& maxCheckpoint) {
  Result result;

  // The Storage Facility uses a timestamp as checkpoint
  auto max = maxCheckpoint.value_or(TicksSinceEpoch<std::chrono::milliseconds>(TimeNow() - 1min));

  /* Checksums over cell versions must be calculated in (backward compatible) lexicographic order, e.g.
   * 1. participant-a/column-x/timestamp-1
   * 2. participant-a/column-x/timestamp-2
   * 3. participant-a/column-y/timestamp-1
   * 4. participant-b/column-x/timestamp-1
   */
  for (const auto& participant : fileStore_.participants()) {
    for (const auto& cell : participant->cells()) {
      for (const auto& version : cell->versions()) {
        std::uint64_t validFromMs{ static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(version.validFrom)) };
        if (validFromMs <= max) {
          result.checkpoint = std::max(result.checkpoint, validFromMs);
          result.checksum = this->addCellVersion(result.checksum, version);
        }
      }
    }
  }

  return result;
}

ChecksumChains ChecksumChain::CreateAll(const FileStore& fileStore) {
  ChecksumChains result;
  result.insert(std::make_unique<FilesChecksumChain>(fileStore));
  result.insert(std::make_unique<EntryCountChecksumChain>(fileStore));
  return result;
}

}
