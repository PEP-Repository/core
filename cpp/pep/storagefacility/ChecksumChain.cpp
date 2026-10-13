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
  using ChecksumChain::ChecksumChain;
};

class EntryCountChecksumChain : public ChecksumChain {
protected:
  virtual uint64_t addCellVersion(uint64_t checksum, const FileStore::CellVersion& version) const override { return ++checksum; }

public:
  using ChecksumChain::ChecksumChain;
};

template <typename TChain>
std::unique_ptr<ChecksumChain> CreateChecksumChain(const FileStore& fileStore) {
  return std::make_unique<TChain>(fileStore);
}

using ChecksumChainFactory = std::unique_ptr<ChecksumChain>(*)(const FileStore& fileStore);

std::map<std::string, ChecksumChainFactory> checksumChainFactories{
  { "entry-count", &CreateChecksumChain<EntryCountChecksumChain>},
  { "files", &CreateChecksumChain<FilesChecksumChain>},
};

}

using namespace std::chrono_literals;

ChecksumChain::ChecksumChain(const FileStore& fileStore) noexcept
  : fileStore_(fileStore) {
}

void ChecksumChain::calculate(std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint) const {
  checksum = 0;
  checkpoint = 0;

  if (!maxCheckpoint) {
    // The Storage Facility uses a timestamp as checkpoint
    maxCheckpoint = TicksSinceEpoch<std::chrono::milliseconds>(TimeNow() - 1min);
  }

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
        if (validFromMs <= *maxCheckpoint) {
          checkpoint = std::max(checkpoint, validFromMs);
          checksum = this->addCellVersion(checksum, version);
        }
      }
    }
  }
}

std::set<std::string> ChecksumChain::Names() {
  return checksumChainFactories
    | std::views::keys
    | std::ranges::to<std::set>();
}

std::unique_ptr<ChecksumChain> ChecksumChain::Create(const FileStore& fileStore, const std::string& name) {
  try {
    return checksumChainFactories.at(name)(fileStore);
  }
  catch (const std::out_of_range&) {
    throw Error("Unknown checksumchain");
  }
}

}
