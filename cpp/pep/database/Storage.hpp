#pragma once

#include <pep/database/Record.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <sqlite_orm/sqlite_orm.h>

namespace pep::database {

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

  /// Return whether any non-tombstone records exist without retrieving them.
  ///
  /// Example: check if a column group is not empty
  /// \code
  ///   myStorage->currentRecordExists<ColumnGroupColumnRecord>(storage,
  ///     c(&ColumnGroupColumnRecord::columnGroup) == columnGroup)
  /// \endcode
  template <Record RecordType>
  [[nodiscard]] bool currentRecordExists(auto whereCondition);

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
  using namespace sqlite_orm;
  auto result = raw.iterate(select(
    columns(max(&RecordType::seqno)),
    where(std::move(whereCondition)),
    std::apply(PEP_WrapOverloadedFunction(group_by), RecordType::RecordIdentifier)
    // SQLite will pick this column from the row with the max() value:
    // https://www.sqlite.org/lang_select.html#bareagg
    .having(c(&RecordType::tombstone) == false),
    limit(1)
  ));
  return result.begin() != result.end();
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
    std::apply(PEP_WrapOverloadedFunction(group_by), RecordType::RecordIdentifier)
    .having(c(&RecordType::tombstone) == false)
  )) | std::views::transform([](auto tuple) { return TryUnwrapTuple(TupleTail(std::move(tuple))); });
}

}
