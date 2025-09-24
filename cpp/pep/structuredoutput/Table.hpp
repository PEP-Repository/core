#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pep::structuredOutput {

/// A densely populated rectangular table of strings, stored in contiguous memory.
/// @details Designed as a simple and generic structure that we can translate to various output formats.
class Table final {
public:

  /// Reference to the fields that make up a single record
  /// @warning Calling a non-const member function of Table can invalidate existing ConstRecordRef objects that
  /// point into that table, because potential data reallocation.
  using ConstRecordRef = std::span<const std::string>;

  /// Reference to the fields that make up a single record
  /// @warning Calling a non-const member function of Table can invalidate existing ConstRecordRef objects that
  /// point into that table, because potential data reallocation.
  using RecordRef = std::span<std::string>;

  /// Creates an empty table from just a header
  /// @throws std::runtime_error if \p header is empty
  static Table EmptyWithHeader(std::vector<std::string> header) {
    const auto rowSize = header.size(); // because we want to pass the size as it was before moving
    return Table(std::move(header), {}, rowSize);
  }

  /// Creates a table by cutting up a flat vector of strings into records that match the size of the header
  /// @throws std::runtime_error if \p header is empty
  /// @throws std::runtime_error if the size of \p data is not a multiple of the size of \p header
  static Table FromSeparateHeaderAndData(std::vector<std::string> header, std::vector<std::string> data) {
    const auto rowSize = header.size(); // because we want to pass the size as it was before moving
    return Table(std::move(header), std::move(data), rowSize);
  }

  /// The name of every column in the table in order
  ConstRecordRef header() const noexcept { return {ConstRecordRef{mHeader.begin(), mHeader.end()}}; }

  /// The name of every column in the table in order
  RecordRef header() noexcept { return asMutable(std::as_const(*this).header()); }

  /// All records in the table.
  /// @details The header is not considered to be a record and is thus not included.
  std::vector<ConstRecordRef> records() const noexcept;

  /// All records in the table.
  /// @details The header is not considered to be a record and is thus not included.
  std::vector<RecordRef> records() noexcept { return asMutable(std::as_const(*this).records()); }

  /// The number of records in the table
  std::size_t size() const noexcept { return mData.size() / mRecordSize; }

  /// The number of records that the table can hold before needing to reallocate memory
  std::size_t capacity() const noexcept { return mData.capacity() / mRecordSize; }

  /// The number of fields in every record
  std::size_t recordSize() const noexcept { return mRecordSize; }

  /// True if there are no records in the table
  bool empty() const noexcept { return mData.empty(); }

  /// Ensures that enough memory is allocated for at least \p n records.
  /// @warning May reallocate internal data and invalidate ConstRecordRef objects that point into this Table
  void reserve(std::size_t n) { mData.reserve(n * mRecordSize); }

  /// Appends a new record to the end of the table
  /// @return A reference to the record that was created
  /// @throws std::runtime_error if the size of \p record in not equal to the recordSize
  /// @warning May reallocate internal data and invalidate ConstRecordRef objects that point into this Table
  RecordRef emplace_back(std::vector<std::string> record);

private:
  Table(std::vector<std::string> header, std::vector<std::string> data, std::size_t recordSize);

  /// Remove const qualification from a reference to internal data
  /// @pre ConstRecordRef must point into mData
  RecordRef asMutable(ConstRecordRef);

  /// Remove const qualification from references to internal data
  /// @pre every ConstRecordRef must point into mData
  std::vector<RecordRef> asMutable(const std::vector<ConstRecordRef>&);

  std::vector<std::string> mHeader; ///< name of each column
  std::vector<std::string> mData;   ///< all fields in row-major order
  std::size_t mRecordSize;          ///< number of fields in each record
};

/// Calls \p func on every field in column \p col of \p table
inline void ForEachFieldInColumn(Table& table, std::size_t col, std::function<void(std::string&)> func) {
  for (auto& record : table.records()) { func(record[col]); }
}

/// Returns true iff \p pred returns true for all fields in column \p col of \p table
inline bool AllOfFieldsInColumn(const Table& table, std::size_t col, std::function<bool(const std::string&)> pred) {
  return std::ranges::all_of(table.records(), [&](const auto& record) { return pred(record[col]); });
}

} // namespace pep::structuredOutput
