#pragma once

#include <pep/storagefacility/StorageFacility.hpp>

namespace pep {

class StorageFacility::SqliteStorage final {
private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

  void deletePurgeCycleRecord(int64_t id);

public:
  explicit SqliteStorage(std::filesystem::path file);
  ~SqliteStorage() noexcept; // Can't be defined here because the destructor for Implementor is unknown

  PurgeCycleId createPurgeCycle(PurgeCycleDefinition definition);
  void deletePurgeCycle(PurgeCycleId id);
};

}
