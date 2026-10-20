#pragma once

#include <pep/storagefacility/FileStore.hpp>

namespace pep {

class SfChecksumChain : public ChecksumChain {
private:
  const FileStore& fileStore_;

protected:
  explicit SfChecksumChain(const FileStore& fileStore, std::string name) noexcept;

  virtual Result addCellVersion(Result result, const FileStore::CellVersion& version) const = 0;

public:
  Result get(const std::optional<Checkpoint>& maxCheckpoint) const override;

  static ChecksumChains CreateAll(const FileStore& fileStore);
};

}
