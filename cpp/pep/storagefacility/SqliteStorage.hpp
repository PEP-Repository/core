#pragma once

#include <pep/storagefacility/PurgeCycle.hpp>
#include <pep/utils/Action.hpp>
#include <filesystem>
#include <mutex>
#include <set>

namespace pep::storagefacility {

class SqliteStorage final {
private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

  // This class needs to be thread safe because it's accessed from Storage Facility's (primary) thread as well as from
  // FileStore's purge persistence thread. We'd have liked to use a reader/writer lock instead of a mutex, but it's
  // unclear which of Implementor::raw's sqlite_orm operations would be safe for concurrent (reader) access. And
  // sqlite_orm's maintainer advises "using your own mutex before accessing a database": see
  // https://github.com/fnc12/sqlite_orm/issues/163#issuecomment-405028656
  // We use a recursive_mutex (as opposed to a non-recursive one) so that there won't be trouble when
  // - methods of this class call each other, and/or
  // - an external caller synchronizes over multiple method calls (each of which will also synchronize).
  mutable std::recursive_mutex mutex_;
  mutable unsigned transactionLevel_ = 0U;

  void performAtomically(const Action& action) const;

  std::optional<int64_t> getRowId(const std::string& name) const;
  std::optional<int64_t> getColumnId(const std::string& name) const;
  int64_t ensureRowId(const std::string& name);
  int64_t ensureColumnId(const std::string& name);

  void deletePurgeCycleRecord(int64_t id);

public:
  struct CellSpec;
  struct CellVersionSpec;
  class ConstraintViolation;

  explicit SqliteStorage(std::filesystem::path file);
  ~SqliteStorage() noexcept; // Can't be defined here because the destructor for Implementor is unknown

  std::vector<PurgeCycle> getPurgeCycles() const;
  PurgeCycle::Id createPurgeCycle(const PurgeCycle& cycle);
  PurgeCycle getPurgeCycle(PurgeCycle::Id id) const;
  void updatePurgeCycle(const PurgeCycle& cycle);
  void deletePurgeCycle(PurgeCycle::Id id);

  void addToPurgeCycle(PurgeCycle::Id id, const CellVersionSpec& cellVersion);

  /// \brief Returns timestamps ("versions") for the specified cell that are included in the specified purge cycle
  /// \param id the purge cycle ID
  /// \param cell the cell to inspect
  /// \return timestamps ("versions") for the specified cell that are included in the specified purge cycle
  std::set<Timestamp> getPurgedTimestamps(PurgeCycle::Id id, const CellSpec& cell) const;

  /// \brief Returns the ("confirmed") timestamp of the purge cycle that includes the specified cell version
  /// \param cellVersion the cell version to inspect
  /// \return the timestamp of the cycle in which the specified cell version is/was/will-be purged, or std::nullopt if the cell version isn't included in a purge cycle
  std::optional<Timestamp> pendingPurgeTimestamp(const CellVersionSpec& cellVersion) const;

  /// \brief Executes a function while providing (exclusive) access to this instance
  /// \tparam TFunction The type of function to execute
  /// \tparam TArgs Types of parameters that will be passed to the function
  /// \param function The function to execute
  /// \param args Parameters to pass to the function
  /// \return The function's return value
  template <typename TFunction, typename... TArgs>
  decltype(auto) atomically(TFunction&& function, TArgs&&... args) const {
    ActionPerformer performer = std::bind(&SqliteStorage::performAtomically, this, std::placeholders::_1);
    return PerformAsAction(performer, std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }
};

class SqliteStorage::ConstraintViolation : public std::runtime_error {
public:
  ConstraintViolation();
};

struct SqliteStorage::CellSpec {
  std::string row; // (Text representation of) the row's local pseudonym for Storage Facility
  std::string column;
};

struct SqliteStorage::CellVersionSpec {
  CellSpec cell;
  Timestamp timestamp;
};

}
