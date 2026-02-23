#pragma once

#include <pep/database/Record.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <sqlite_orm/sqlite_orm.h>

namespace pep::database {
struct SchemaError : std::logic_error {
  enum class Reason {
    dropped_and_recreated,
    old_columns_removed,
  };
  SchemaError(std::string table, Reason reason);

  std::string mTable;
  Reason mReason;
};

template<typename T>
struct having {
  explicit having(T&& expr) : mExpr(std::move(expr)) {}
  T mExpr;
};

/// @brief Non-template base class for Storage<> (defined below).
struct BasicStorage {
  /// @brief Whether the storage is stored on a persistent medium (TRUE) or in memory (FALSE)
  const bool isPersistent;

  /// @brief Specify this as the "path" to construct a Storage<> that's non-persistent, i.e. backed by memory
  static const char* const STORE_IN_MEMORY;

private:
  template <auto MakeRaw> friend struct Storage;
  explicit BasicStorage(const std::string& path);
};

/* !
 * \brief Helper for storage using sqlite-orm.
 * \tparam MakeRaw A function that accepts an std::string and returns the result of invoking sqlite_orm::make_storage with that string.
 *
 * \code
 *   struct Person { std::string name };
 *
 *   auto MakeRawStorage(std::string path) {
 *     using namespace sqlite_orm;
 *     return make_storage(std::move(path),
 *       make_table("People",
 *         make_column("name", &Person::name)));
 *   }
 *
 *   using MyStorage = pep::database::Storage<MakeRawStorage>;
 * \endcode
 *
 * \remark A.o. usable to hide storage details behind the pimpl idiom:
 *         - in the .hpp, declare (but don't define) a MyStorage class.
 *         - in the .cpp, define MyStorage to e.g. inherit from pep::database::Storage<> instead of being a typedef/alias.
 */
template <auto MakeRaw>
struct Storage : public BasicStorage {
  /// @brief The raw sqlite_orm storage type
  using Raw = decltype(MakeRaw(std::string()));

  /// @brief The raw sqlite_orm storage
  Raw raw;

  /// @brief Constructor
  /// @param path The path to the sqlite database file. Pass STORE_IN_MEMORY to initialize non-persistent storage.
  explicit Storage(std::string path)
    : BasicStorage(path), raw(MakeRaw(std::move(path))) {}

  /// \brief Sync the database schema if that causes no data loss. Throws an error otherwise.
  /// \param allow_old_column_removal Whether removal of old columns is allowed. When set to true, columns that are in the database, but not in the `make_storage` call, will be removed. If set to false, this will produce an error
  /// \throws SchemaError if syncing the schema would cause a table being dropped, or if \p allow_old_column_removal is false and one or more columns would be dropped.
  /// \throws std::system_error if sqlite produces errors
  /// \returns true if changes have been made, false if the whole database schema was already in sync.
  bool syncSchema(bool allow_old_column_removal = false) {
    LOG("database::Storage", info) << "Syncing database schema...";
    try {
      auto simulateResults = raw.sync_schema_simulate(true);
      for(const auto& [tableName, result] : simulateResults) {
        switch (result) {
        case sqlite_orm::sync_schema_result::already_in_sync:
        case sqlite_orm::sync_schema_result::new_table_created:
        case sqlite_orm::sync_schema_result::new_columns_added:
          break;
        case sqlite_orm::sync_schema_result::dropped_and_recreated:
          throw SchemaError(tableName, SchemaError::Reason::dropped_and_recreated);
        case sqlite_orm::sync_schema_result::old_columns_removed:
        case sqlite_orm::sync_schema_result::new_columns_added_and_old_columns_removed:
          if (allow_old_column_removal)
            break;
          throw SchemaError(tableName, SchemaError::Reason::old_columns_removed);
        }
      }
      auto syncResults  = raw.sync_schema(true);
      assert(syncResults == simulateResults);
      return std::ranges::find_if(syncResults,
        [](auto result){ return result.second != sqlite_orm::sync_schema_result::already_in_sync; }) != syncResults.end();
    }
    catch (const std::system_error& e) {
      LOG("database::Storage", error) << "  failed: " << e.what();
      throw;
    }
  }

  /// Return whether any non-tombstone records exist without retrieving them.
  ///
  /// Example: check if a column group is not empty
  /// \code
  ///   myStorage->currentRecordExists<ColumnGroupColumnRecord>(
  ///     c(&ColumnGroupColumnRecord::columnGroup) == columnGroup)
  /// \endcode
  template <Record RecordType>
  [[nodiscard]] bool currentRecordExists(auto whereCondition);

  /// Return whether any non-tombstone records exist without retrieving them.
  /// The where-clause is evaluated for all records, before determining which records are current. The having-clause is only evaluated for the current records.
  ///
  /// Example: check if a UserGroupRecord with a certain name exists
  /// The name has to be checked in the having-clause. We first need to decide which records are current, before checking the name.
  /// Otherwise records that used to have the given name, but no longer do, will match the query.
  /// Even worse, if the record is tombstoned with a different name, that tombstone record will be eliminated before checking which records are current.
  /// So the method would incorrectly return true.
  /// \code
  ///   myStorage->currentRecordExists<UserGroupRecord>(
  ///     true,
  ///     having(c(&UserGroupRecord::name) == name))
  /// \endcode
  template <Record RecordType, typename havingT>
  [[nodiscard]] bool currentRecordExists(auto whereCondition, having<havingT> havingCondition);


  /// Return non-tombstone records.
  /// The where-clause is evaluated for all records, before determining which records are current. The having-clause is only evaluated for the current records.
  ///
  /// Example: get the current primary identifier for a given internalUserId.
  /// If we check `isPrimaryId` in the where-clause, we filter out all the records that tell us that a current record is no longer the primary ID.
  /// So it would return all records that have ever been the primary ID, instead of only the records that are currently the primary ID.
  /// \code
  ///   using namespace pep::database;
  ///   myStorage->getCurrentRecords(storage,
  ///     c(&UserIdRecord::timestamp) <= timestamp.getTime()
  ///       && c(&UserIdRecord::internalUserId) == internalUserId,
  ///     having(c(&UserIdRecord::isPrimaryId) == true),
  ///     &UserIdRecord::identifier
  /// \endcode
  ///
  /// \returns Collection of tuples with columns from \p selectColumns
  ///   (or single values if a single column was specified)
  template <Record RecordType, typename havingT, typename... ColTypes>
  [[nodiscard]] auto getCurrentRecords(auto whereCondition, having<havingT> havingCondition, ColTypes RecordType::*... selectColumns);

  /// Return non-tombstone records.
  ///
  /// Example: enumerate all metadata for a specific subject type
  /// \code
  ///   myStorage->getCurrentRecords(storage,
  ///     c(&MetadataRecord::timestamp) <= TicksSinceEpoch<std::chrono::milliseconds>(timestamp)
  ///     && c(&MetadataRecord::subjectType) == subjectType,
  ///     &MetadataRecord::subject,
  ///     &MetadataRecord::metadataGroup,
  ///     &MetadataRecord::subkey,
  ///     &MetadataRecord::value)
  /// \endcode
  ///
  /// \returns Collection of tuples with columns from \p selectColumns
  ///   (or single values if a single column was specified)
  template <Record RecordType, typename... ColTypes>
  [[nodiscard]] auto getCurrentRecords(auto whereCondition, ColTypes RecordType::*... selectColumns);
};

template <auto MakeRaw> template <Record RecordType>
[[nodiscard]] bool Storage<MakeRaw>::currentRecordExists(auto whereCondition) {
  return currentRecordExists<RecordType>(whereCondition, having(true));
}

template <auto MakeRaw> template <Record RecordType, typename havingT>
[[nodiscard]] bool Storage<MakeRaw>::currentRecordExists(auto whereCondition, having<havingT> havingCondition) {
  using namespace sqlite_orm;
  auto result = raw.iterate(select(
    columns(max(&RecordType::seqno)),
    where(std::move(whereCondition)),
    std::apply(PEP_WrapFn(group_by), RecordType::RecordIdentifier)
    // SQLite will pick this column from the row with the max() value:
    // https://www.sqlite.org/lang_select.html#bareagg
    .having(c(&RecordType::tombstone) == false && havingCondition.mExpr),
    limit(1)
  ));
  return result.begin() != result.end();
}

template <auto MakeRaw> template <Record RecordType, typename havingT, typename... ColTypes>
[[nodiscard]] auto Storage<MakeRaw>::getCurrentRecords(auto whereCondition, having<havingT> havingCondition, ColTypes RecordType::*... selectColumns) {
  static_assert(sizeof...(ColTypes) > 0, "No columns specified");
  using namespace sqlite_orm;
  return raw.iterate(select(
    // SQLite will pick these columns from the row with the max() value:
    // https://www.sqlite.org/lang_select.html#bareagg
    columns(max(&RecordType::seqno), selectColumns...),
    where(whereCondition),
    std::apply(PEP_WrapFn(group_by), RecordType::RecordIdentifier)
    .having(c(&RecordType::tombstone) == false && havingCondition.mExpr)
  )) | std::views::transform([](auto tuple) { return TryUnwrapTuple(TupleTail(std::move(tuple))); });
}

template <auto MakeRaw> template <Record RecordType, typename... ColTypes>
[[nodiscard]] auto Storage<MakeRaw>::getCurrentRecords(auto whereCondition, ColTypes RecordType::*... selectColumns) {
  static_assert(sizeof...(ColTypes) > 0, "No columns specified");
  using namespace sqlite_orm;
  return raw.iterate(select(
    // SQLite will pick these columns from the row with the max() value:
    // https://www.sqlite.org/lang_select.html#bareagg
    columns(max(&RecordType::seqno), selectColumns...),
    where(whereCondition),
    std::apply(PEP_WrapFn(group_by), RecordType::RecordIdentifier)
    .having(c(&RecordType::tombstone) == false)
  )) | std::views::transform([](auto tuple) { return TryUnwrapTuple(TupleTail(std::move(tuple))); });

}

}
