#pragma once

#include <concepts>
#include <cstdint>
#include <tuple>

namespace pep::database {

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

} // End namespace detail


template <typename T>
concept Record = requires(T rec) {
  { auto(rec.seqno) } -> std::integral;
  { auto(rec.timestamp) } -> std::integral;
  { auto(rec.tombstone) } -> std::same_as<bool>;
  { auto(T::RecordIdentifier) } -> detail::RecordIdentifier<T>;
};

/// Timestamp as milliseconds since Unix epoch, used for in database
using UnixMillis = std::int64_t;

}
