#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <pep/storagefacility/FileStore.hpp>

namespace pep::storagefacility {

class ChecksumChain : public pep::ChecksumChain {
private:
  const FileStore& fileStore_;

protected:
  explicit ChecksumChain(const FileStore& fileStore, std::string name) noexcept;

  Result addCell(Result result, Checkpoint max, const FileStore::Cell& cell);
  virtual Result addCellVersion(Result result, const FileStore::CellVersion& version) = 0;

public:
  Result get(const std::optional<Checkpoint>& maxCheckpoint) override;

  static ChecksumChains CreateAll(const FileStore& fileStore);
};

}
