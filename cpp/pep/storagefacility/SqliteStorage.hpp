#pragma once

#include <pep/storagefacility/StorageFacility.hpp>

namespace pep {

class StorageFacility::SqliteStorage final {
private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

  int64_t getRowId(const std::string& name);
  int64_t getColumnId(const std::string& name);
  void deletePurgeCycleRecord(int64_t id);

public:
  class ConstraintViolation;

  explicit SqliteStorage(std::filesystem::path file);
  ~SqliteStorage() noexcept; // Can't be defined here because the destructor for Implementor is unknown

  std::vector<PurgeCycle> getPurgeCycles() const;
  PurgeCycle::Id createPurgeCycle(const PurgeCycle& cycle);
  PurgeCycle getPurgeCycle(PurgeCycle::Id id) const;
  void updatePurgeCycle(const PurgeCycle& cycle);
  void deletePurgeCycle(PurgeCycle::Id id);

  void addToPurgeCycle(PurgeCycle::Id id, const std::string& row, const std::string& column, Timestamp timestamp);
};

class StorageFacility::SqliteStorage::ConstraintViolation : public std::runtime_error {
public:
  ConstraintViolation();
};

}
