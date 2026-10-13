#include <pep/storagefacility/ChecksumChain.hpp>
#include <pep/serialization/Error.hpp>

namespace pep::storagefacility {

namespace {

class FilesChecksumChain : public ChecksumChain {
protected:
  virtual Result addCellVersion(Result result, const FileStore::CellVersion& version) override {
    result.checksum ^= version.checksumSubstitute;
    // result.checkpoint is updated by this->addCell (who is calling us)
    return result;
  }

public:
  explicit FilesChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "files") {}
};

class EntryCountChecksumChain : public ChecksumChain {
protected:
  virtual Result addCellVersion(Result result, const FileStore::CellVersion& version) override {
    result.checksum++;
    // result.checkpoint is updated by this->addCell (who is calling us)
    return result;
  }

public:
  explicit EntryCountChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "entry-count") {}
};

ChecksumChain::Checkpoint TimestampToCheckpoint(Timestamp timestamp) {
  return static_cast<ChecksumChain::Checkpoint>(TicksSinceEpoch<std::chrono::milliseconds>(timestamp));
}

}

using namespace std::chrono_literals;

ChecksumChain::ChecksumChain(const FileStore& fileStore, std::string name) noexcept
  : pep::ChecksumChain(std::move(name)), fileStore_(fileStore) {
}

ChecksumChain::Result ChecksumChain::addCell(Result result, Checkpoint max, const FileStore::Cell& cell) {
  for (const auto& version : cell.versions()) {
    auto versionCheckpoint = TimestampToCheckpoint(version.validFrom);
    if (versionCheckpoint <= max) {
      result = this->addCellVersion(result, version);
      result.checkpoint = std::max(result.checkpoint, versionCheckpoint); // Saves duplicate code in this->addCellVersion
    }
  }
  return result;
}

ChecksumChain::Result ChecksumChain::get(const std::optional<Checkpoint>& maxCheckpoint) {
  Result result;

  // The Storage Facility uses a timestamp as checkpoint
  auto max = maxCheckpoint.value_or(TimestampToCheckpoint(TimeNow() - 1min));

  /* Checksums over cell versions must be calculated in (backward compatible) lexicographic order, e.g.
   * 1. participant-a/column-x/timestamp-1
   * 2. participant-a/column-x/timestamp-2
   * 3. participant-a/column-y/timestamp-1
   * 4. participant-b/column-x/timestamp-1
   */
  for (const auto& participant : fileStore_.participants()) {
    for (const auto& cell : participant->cells()) {
      result = this->addCell(result, max, *cell);
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
