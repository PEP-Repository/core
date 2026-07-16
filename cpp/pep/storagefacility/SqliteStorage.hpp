#pragma once

#include <pep/storagefacility/StorageFacility.hpp>

namespace pep {

class StorageFacility::SqliteStorage final {
private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

public:
  explicit SqliteStorage(std::filesystem::path file);
  ~SqliteStorage() noexcept; // Can't be defined here because the destructor for Implementor is unknown

  PurgeCycleId startPurgeCycle(std::string name);
  void cancelPurge(PurgeCycleId id);
};

}
