#include <pep/storagefacility/ChecksumChain.hpp>
#include <pep/serialization/Error.hpp>

namespace pep::storagefacility {

namespace {

ChecksumChain::Checkpoint TimestampToCheckpoint(Timestamp timestamp) {
  return static_cast<ChecksumChain::Checkpoint>(TicksSinceEpoch<std::chrono::milliseconds>(timestamp));
}

class FilesChecksumChain : public ChecksumChain {
private:
  struct PurgeRecord {
    // Must be ordered deterministically to prevent checksum chain calculation differences, and since...
    Checkpoint purgedAt; // ... multiple cell versions may have been purged at the same time (as part of the same cycle)...
    Checkpoint validFrom; // ...we include the (unique) timestamp of the cell version itself

    Checksum checksum() const { return purgedAt ^ validFrom; }

    std::strong_ordering operator<=>(const PurgeRecord&) const = default;
  };

  std::set<PurgeRecord> purgeRecords_;

  Result addPurgeChecksums(Result result, Checkpoint max) {
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
  Result addCell(Result result, Checkpoint max, const FileStore::Cell& cell) override {
    assert(purgeRecords_.empty() && "State wasn't cleared by processing of previous cell");

    // Let base class implementation (invoke our method to) process cell versions
    result = ChecksumChain::addCell(result, max, cell);
    // Process any purges that occurred (after the last cell version but) before the max checkpoint
    result = this->addPurgeChecksums(result, max);
    // Discard remaining purge records for this cell, to clear the state for (processing of) the next cell
    purgeRecords_.clear();

    return result;
  }

  Result addCellVersion(Result result, const FileStore::CellVersion& version) override {
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
      purgeRecords_.emplace(PurgeRecord{
        .purgedAt = TimestampToCheckpoint(*version.purgedAt),
        .validFrom = TimestampToCheckpoint(version.validFrom),
        });
    }

    return result;
  }

public:
  explicit FilesChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "files") {}
};

class EntryCountChecksumChain : public ChecksumChain {
protected:
  Result addCellVersion(Result result, const FileStore::CellVersion& version) override {
    result.checksum++;
    // result.checkpoint is updated by this->addCell (who is calling us)
    return result;
  }

public:
  explicit EntryCountChecksumChain(const FileStore& fileStore) : ChecksumChain(fileStore, "entry-count") {}
};

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
