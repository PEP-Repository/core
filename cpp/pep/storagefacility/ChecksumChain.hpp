#pragma once

#include <pep/storagefacility/FileStore.hpp>

namespace pep::storagefacility {

class ChecksumChain {
private:
  const FileStore& fileStore_;

protected:
  virtual uint64_t addCellVersion(uint64_t checksum, const FileStore::CellVersion& version) const = 0;

public:
  explicit ChecksumChain(const FileStore& fileStore) noexcept;

  void calculate(std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint) const;

  static std::set<std::string> Names();
  static std::unique_ptr<ChecksumChain> Create(const FileStore& fileStore, const std::string& name);
};

}
