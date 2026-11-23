#pragma once

#include <pep/storagefacility/EntryName.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>
#include <pep/utils/Action.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/Timestamp.hpp>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

namespace pep::storagefacility {

/// SQLite database (file) frontend for Storage Facility.
/// \remark Public methods are thread safe. (Private methods require caller to synchronize.)
///         Callers can synchronize (over multiple method calls) by means of the "atomically" method.
class SqliteStorage final : public SharedConstructor<SqliteStorage> {
  friend class SharedConstructor<SqliteStorage>;

private:
  struct Implementor;
  std::unique_ptr<Implementor> implementor_;

  // This class needs to be thread safe because it's accessed from Storage Facility's (primary) thread as well as from
  // FileStore's redaction persistence thread. We'd have liked to use a reader/writer lock instead of a mutex, but it's
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

  explicit SqliteStorage(std::filesystem::path file);

public:
  struct CellVersionSpec;
  class ConstraintViolation;

  ~SqliteStorage() noexcept; // Can't be defined here because the destructor for Implementor is unknown

  std::vector<RedactionCycle> getRedactionCycles() const;
  RedactionCycle::Id createRedactionCycle(const RedactionCycle& cycle);
  RedactionCycle getRedactionCycle(RedactionCycle::Id id) const;
  void updateRedactionCycle(const RedactionCycle& cycle);
  void deleteRedactionCycle(RedactionCycle::Id id);

  void addToRedactionCycle(RedactionCycle::Id id, const CellVersionSpec& cellVersion);

  /// \brief Returns cell version(specification)s that are included in the specified redaction cycle.
  /// \param id the redaction cycle ID
  /// \return cell version(specification)s that are included in the specified redaction cycle.
  std::set<CellVersionSpec> getIncludedCellVersions(RedactionCycle::Id id) const;

  /// \brief Executes a function while providing (exclusive, thread safe and transactional) access to this instance
  /// \tparam TFunction The type of function to execute
  /// \tparam TArgs Types of parameters that will be passed to the function
  /// \param function The function to execute
  /// \param args Parameters to pass to the function
  /// \return The function's return value
  /// \remark Any changes (that the function makes) to this SqliteStorage instance will be rolled back if an exception escapes.
  template <typename TFunction, typename... TArgs>
  decltype(auto) atomically(TFunction&& function, TArgs&&... args) const {
    return PerformAsAction(std::bind_front(&SqliteStorage::performAtomically, this), std::forward<TFunction>(function), std::forward<TArgs>(args)...);
  }
};

class SqliteStorage::ConstraintViolation : public std::runtime_error {
public:
  ConstraintViolation();
};

struct SqliteStorage::CellVersionSpec {
  EntryName cell;
  Timestamp timestamp;

  auto operator<=>(const CellVersionSpec&) const = default;
};

}
