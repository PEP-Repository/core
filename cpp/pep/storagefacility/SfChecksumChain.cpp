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
 * Purges of cell versions are processed at the timestamp when the purge cycle was applied: e.g. if
 * participant-a/column-y/timestamp-1 (entry 3) was purged
 * - at timestamp 2, the purge is processed between entries 3 and 4.
 * - at timestamp 4, the purge is processed between entries 4 and 5.
 */
class FilesChecksumChain : public SfChecksumChain {
private:
  struct PurgeRecord {
    // Must be ordered deterministically to prevent checksum chain calculation differences, and since...
    Checkpoint purgedAt; // ... multiple cell versions may have been purged at the same time (as part of the same cycle)...
    Checkpoint validFrom; // ...we include the (unique) timestamp of the cell version itself

    Checksum checksum() const { return purgedAt ^ validFrom; } // Just a unique value for the purging of this cell version

    std::strong_ordering operator<=>(const PurgeRecord&) const = default;
  };

  mutable std::set<PurgeRecord> purgeRecords_;

  Result addPurgeChecksums(Result result, Checkpoint max) const {
    while (!purgeRecords_.empty()) {
      auto first = purgeRecords_.begin();
      const auto& record = *first;
      if (record.purgedAt > max) {
        assert(std::ranges::all_of(purgeRecords_, [max](const PurgeRecord& rec) {return rec.purgedAt > max; }));
        break;
      }

      result.checksum ^= record.checksum();
      result.checkpoint = std::max(result.checkpoint, record.purgedAt);
      purgeRecords_.erase(first);
    }

    return result;
  }

protected:
  Result addCell(Result result, Checkpoint max, const FileStore::Cell& cell) const override {
    assert(purgeRecords_.empty() && "State wasn't cleared by processing of previous cell");

    // Let base class implementation (invoke our method to) process cell versions
    result = SfChecksumChain::addCell(result, max, cell);
    // Process any purges that occurred (after the last cell version but) before the max checkpoint
    result = this->addPurgeChecksums(result, max);
    // Discard remaining purge records for this cell, to clear the state for (processing of) the next cell
    purgeRecords_.clear();

    return result;
  }

  Result addCellVersion(Result result, const FileStore::CellVersion& version) const override {
    // Process any purges that occurred before this cell version was stored.
    // Defensive programming: we subtract 1 from the version's timestamp to process only purges that occurred **before**
    // this version appeared, even though SF already ensures that purge cycle timestamps don't collide with storage
    // timestamps.
    result = this->addPurgeChecksums(result, TimestampToCheckpoint(version.validFrom) - 1);

    // Process this cell version itself
    result.checksum ^= version.checksumSubstitute;
    // result.checkpoint is updated by this->addCell (who is calling us)

    // If this cell version was purged, record the timestamp so the purge can be processed into the checksum chain at
    // the appropriate checkpoint
    if (version.purgedAt.has_value()) {
      assert(*version.purgedAt > version.validFrom);
      [[maybe_unused]] auto emplaced = purgeRecords_.emplace(PurgeRecord{
        .purgedAt = TimestampToCheckpoint(*version.purgedAt),
        .validFrom = TimestampToCheckpoint(version.validFrom),
        }).second;
      assert(emplaced);
    }

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
