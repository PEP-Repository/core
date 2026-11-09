#include <pep/storagefacility/SfChecksumChain.hpp>
#include <pep/serialization/Error.hpp>

namespace pep {

namespace {

ChecksumChain::Checkpoint TimestampToCheckpoint(Timestamp timestamp) {
  return static_cast<ChecksumChain::Checkpoint>(TicksSinceEpoch<std::chrono::milliseconds>(timestamp));
}

/* Checksums over cell versions must be calculated in (backward compatible) lexicographic order, e.g.
 * 1. participant-a/column-x/timestamp-1
 * 2. participant-a/column-x/timestamp-2
 * 3. participant-a/column-y/timestamp-1
 * 4. participant-a/column-y/timestamp-3
 * 5. participant-a/column-y/timestamp-5
 * 6. participant-b/column-x/timestamp-1
 * Purges of cell versions are processed after cell version contents.
 */
class FilesChecksumChain : public SfChecksumChain {
protected:
  Result addCell(Result result, Checkpoint max, const FileStore::Cell& cell) const override {
    /*
    * Persistent issues:
    * now:  StorageFacility: checksum for chain files@000001a1ac85aebf changed: 59421e21e74d01be != 59421e21e7fd3db9
    */

    // Let base class implementation (invoke our method to) process cell version(content)s
    result = SfChecksumChain::addCell(result, max, cell);

    // Process any purges that occurred before the max checkpoint
    for (const FileStore::CellVersion& version : cell.versions()) {
      auto versionCheckpoint = TimestampToCheckpoint(version.validFrom);
      if (versionCheckpoint > max) {
        break;
      }

      if (version.purgedAt) {
        auto purgeCheckpoint = TimestampToCheckpoint(*version.purgedAt);
        if (purgeCheckpoint <= max) {
          auto add = purgeCheckpoint ^ versionCheckpoint; // Just a unique value for the purging of this cell version
          result.checksum ^= add;
          result.checkpoint = std::max(result.checkpoint, purgeCheckpoint);
        }
      }
    }

    return result;
  }

  Result addCellVersion(Result result, const FileStore::CellVersion& version) const override {
    // Process this cell version itself
    result.checksum ^= version.checksumSubstitute;
    // result.checkpoint is updated by this->addCell (who is calling us)

    return result;
  }

public:
  explicit FilesChecksumChain(const FileStore& fileStore) : SfChecksumChain(fileStore, "files") {}
};

class EntryCountChecksumChain : public SfChecksumChain {
protected:
  Result addCellVersion(Result result, const FileStore::CellVersion& version) const override {
    result.checksum++;
    // result.checkpoint is updated by this->addCell (who is calling us)
    return result;
  }

public:
  explicit EntryCountChecksumChain(const FileStore& fileStore) : SfChecksumChain(fileStore, "entry-count") {}
};

}

using namespace std::chrono_literals;

SfChecksumChain::SfChecksumChain(const FileStore& fileStore, std::string name) noexcept
  : ChecksumChain(std::move(name)), fileStore_(fileStore) {
}

ChecksumChain::Result SfChecksumChain::addCell(Result result, Checkpoint max, const FileStore::Cell& cell) const {
  for (const auto& version : cell.versions()) {
    auto versionCheckpoint = TimestampToCheckpoint(version.validFrom);
    if (versionCheckpoint > max) {
      break;
    }

    result = this->addCellVersion(result, version);
    result.checkpoint = std::max(result.checkpoint, versionCheckpoint); // Saves duplicate code in (overrides of) this->addCellVersion
  }
  return result;
}

ChecksumChain::Result SfChecksumChain::get(const std::optional<Checkpoint>& maxCheckpoint) const {
  Result result;

  // The Storage Facility uses a timestamp as checkpoint
  auto max = maxCheckpoint.value_or(TimestampToCheckpoint(TimeNow() - 1min));

  for (const auto& participant : fileStore_.participants()) {
    for (const auto& cell : participant->cells()) {
      result = this->addCell(result, max, *cell);
    }
  }

  return result;
}

ChecksumChains SfChecksumChain::CreateAll(const FileStore& fileStore) {
  return ChecksumChains{
    std::make_unique<FilesChecksumChain>(fileStore),
    std::make_unique<EntryCountChecksumChain>(fileStore),
  };
}

}
