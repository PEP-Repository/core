#pragma once

#include <pep/storagefacility/PurgeCycle.hpp>

namespace pep::storagefacility {

class SqliteStorage final {
private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

  std::optional<int64_t> getRowId(const std::string& name) const;
  std::optional<int64_t> getColumnId(const std::string& name) const;
  int64_t ensureRowId(const std::string& name);
  int64_t ensureColumnId(const std::string& name);

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

  /// @brief Returns timestamps ("versions") for the specified cell that are included in the specified purge cycle
  /// @param id the purge cycle ID
  /// @param row (the text representation of) the row's local pseudonym for Storage Facility
  /// @param column the column name
  /// @return timestamps ("versions") for the specified cell that are included in the specified purge cycle
  std::set<Timestamp> getPurgedTimestamps(PurgeCycle::Id id, const std::string& row, const std::string& column) const;

  /// @brief Returns the ("confirmed") timestamp of the purge cycle that includes the specified cell version
  /// @param row (the text representation of) the row's local pseudonym for Storage Facility
  /// @param column the column name
  /// @param timestamp the cell timestamp ("version")
  /// @return the timestamp of the cycle in which the specified cell version is/was/will-be purged, or std::nullopt if the cell version isn't included in a purge cycle
  std::optional<Timestamp> pendingPurgeTimestamp(const std::string& row, const std::string& column, Timestamp timestamp) const;
};

class SqliteStorage::ConstraintViolation : public std::runtime_error {
public:
  ConstraintViolation();
};

}
