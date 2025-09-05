#pragma once

#include <concepts>
#include <ranges>
#include <utility>

#include <sqlite_orm/sqlite_orm.h>

#include <pep/utils/MiscUtil.hpp>

namespace pep {
namespace detail {
  template <typename Record, typename Ids>
  inline constexpr bool IsRecordIdentifier = false;
  template <typename Record, typename... IdTypes>
  inline constexpr bool IsRecordIdentifier<Record, std::tuple<IdTypes Record::*...>> = true;

  /// The RecordIdentifier field contains columns that identify a thing in the database.
  /// If the tombstone column is true, then all records with columns equal to these identifying columns are tombstoned.
  /// E.g. a name will probably be identifying, but a description probably not.
  template <typename Ids, typename Record>
  concept RecordIdentifier = IsRecordIdentifier<Record, Ids>;
}

template <typename Record>
concept DatabaseRecord = requires(Record rec) {
  { decay_copy(rec.seqno) } -> std::integral;
  { decay_copy(rec.timestamp) } -> std::integral;
  { decay_copy(rec.tombstone) } -> std::same_as<bool>;
  { decay_copy(Record::RecordIdentifier) } -> detail::RecordIdentifier<Record>;
};

/// Return whether any non-tombstone records exist without retrieving them.
///
/// Example: check if a column group is not empty
/// \code
///   CurrentRecordExists<ColumnGroupColumnRecord>(storage,
///     c(&ColumnGroupColumnRecord::columnGroup) == columnGroup)
/// \endcode
template <DatabaseRecord Record>
[[nodiscard]] bool CurrentRecordExists(auto& storage, auto whereCondition) {
  using namespace sqlite_orm;
  auto result = storage.iterate(select(
    columns(max(&Record::seqno)),
    where(std::move(whereCondition)),
    std::apply(PEP_WrapOverloadedFunction(group_by), Record::RecordIdentifier)
        // SQLite will pick this column from the row with the max() value:
        // https://www.sqlite.org/lang_select.html#bareagg
        .having(c(&Record::tombstone) == false),
    limit(1)
  ));
  return result.begin() != result.end();
}

/// Return non-tombstone records.
///
/// Example: enumerate all metadata for a specific subject type
/// \code
///   GetCurrentRecords(storage,
///     c(&MetadataRecord::timestamp) <= timestamp.getTime()
///     && c(&MetadataRecord::subjectType) == subjectType,
///     &MetadataRecord::subject,
///     &MetadataRecord::metadataGroup,
///     &MetadataRecord::subkey,
///     &MetadataRecord::value)
/// \endcode
///
/// \returns Collection of tuples with columns from \p selectColumns
///   (or single values if a single column was specified)
template <DatabaseRecord Record, typename... ColTypes>
[[nodiscard]] auto GetCurrentRecords(auto& storage, auto whereCondition, ColTypes Record::*... selectColumns) {
  static_assert(sizeof...(ColTypes) > 0, "No columns specified");
  using namespace sqlite_orm;
  return storage.iterate(select(
    // SQLite will pick these columns from the row with the max() value:
    // https://www.sqlite.org/lang_select.html#bareagg
    columns(max(&Record::seqno), selectColumns...),
    where(whereCondition),
    std::apply(PEP_WrapOverloadedFunction(group_by), Record::RecordIdentifier)
        .having(c(&Record::tombstone) == false)
  )) | std::views::transform([](auto tuple) { return TryUnwrapTuple(TupleTail(std::move(tuple))); });
}

}
