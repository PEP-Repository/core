#include <pep/database/Storage.hpp>
#include <pep/database/UnixMillis.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <pep/utils/Action.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Timestamp.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <sqlite_orm/sqlite_orm.h>
#include <sqlite3.h>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <system_error>
#include <string>
#include <utility>
#include <vector>

namespace pep::storagefacility {

namespace {

// Normalized storage for row pseudonyms to reduce storage space use
struct RowRecord {
  int64_t seqno{};
  std::string pseudonym; // text representation of the row's local pseudonym for SF
};

// Normalized storage for column names to reduce storage space use
struct ColumnRecord {
  int64_t seqno{};
  std::string name;
};

struct RedactionCycleRecord { //NOLINT(cppcoreguidelines-pro-type-member-init) We want callers to (explicitly) initialize the "announced" field
  RedactionCycle::Id id{};
  std::string name;
  database::UnixMillis announced;
  database::UnixMillis applied{};
  database::UnixMillis completed{};
};

struct RedactedEntryRecord { //NOLINT(cppcoreguidelines-pro-type-member-init) We want callers to (explicitly) initialize the uninitialized fields (cycle, row, column, timestamp)
  int64_t seqno{};
  RedactionCycle::Id cycle; // FK to RedactionCycleRecord::id
  int64_t row;              // FK to RowRecord::seqno
  int64_t column;           // FK to ColumnRecord::seqno
  database::UnixMillis timestamp;
};

auto MakeRawStorage(std::string path) {
  using namespace sqlite_orm;
  return make_storage(std::move(path),
    make_table("Row",
      make_column("seqno", &RowRecord::seqno,
        primary_key().autoincrement()),
      make_column("pseudonym", &RowRecord::pseudonym),
      unique(&RowRecord::pseudonym)),

    make_table("Column",
      make_column("seqno", &ColumnRecord::seqno,
        primary_key().autoincrement()),
      make_column("name", &ColumnRecord::name),
      unique(&ColumnRecord::name)),

    make_table("RedactionCycle",
      make_column("id", &RedactionCycleRecord::id,
        primary_key().autoincrement()),
      make_column("name", &RedactionCycleRecord::name),
      make_column("announced", &RedactionCycleRecord::announced),
      make_column("applied", &RedactionCycleRecord::applied),
      make_column("completed", &RedactionCycleRecord::completed)),

    make_index("idx_RedactedEntry_cycle", &RedactedEntryRecord::cycle),
    make_table("RedactedEntry",
      make_column("seqno", &RedactedEntryRecord::seqno,
        primary_key().autoincrement()),
      make_column("cycle", &RedactedEntryRecord::cycle),
      make_column("row", &RedactedEntryRecord::row),
      make_column("column", &RedactedEntryRecord::column),
      make_column("timestamp", &RedactedEntryRecord::timestamp),
      foreign_key(&RedactedEntryRecord::cycle).references(&RedactionCycleRecord::id),
      foreign_key(&RedactedEntryRecord::row).references(&RowRecord::seqno),
      foreign_key(&RedactedEntryRecord::column).references(&ColumnRecord::seqno),
      unique(
        &RedactedEntryRecord::row,
        &RedactedEntryRecord::column,
        &RedactedEntryRecord::timestamp))
  );
}

std::optional<Timestamp> OptionalTimestampFromUnixMillis(database::UnixMillis millis) {
  if (millis != 0) {
    return database::TimestampFromUnixMillis(millis);
  }
  return std::nullopt;
}

database::UnixMillis OptionalTimestampToUnixMillis(const std::optional<Timestamp>& timestamp) {
  if (timestamp.has_value()) {
    if (*timestamp == Timestamp{ /* zero */ }) {
      throw Error("Absent timestamp requires different representation"); // Should have been nullopt
    }
    return database::TimestampToUnixMillis(*timestamp);
  }
  return 0;
}

RedactionCycle RecordToRedactionCycle(RedactionCycleRecord record) {
  return RedactionCycle{
    .id = record.id,
    .definition = RedactionCycle::Definition(std::move(record.name)),
    .announced = database::TimestampFromUnixMillis(record.announced),
    .applied = OptionalTimestampFromUnixMillis(record.applied),
    .completed = OptionalTimestampFromUnixMillis(record.completed),
  };
}

RedactionCycleRecord RedactionCycleToRecord(const RedactionCycle& cycle) {
  return RedactionCycleRecord{
    .id = cycle.id,
    .name = cycle.definition.name(),
    .announced = database::TimestampToUnixMillis(cycle.announced),
    .applied = OptionalTimestampToUnixMillis(cycle.applied),
    .completed = OptionalTimestampToUnixMillis(cycle.completed),
  };
}

} // End anonymous namespace

SqliteStorage::ConstraintViolation::ConstraintViolation()
  : std::runtime_error("SQLite constraint violation") {
}

struct SqliteStorage::Implementor : public pep::database::Storage<MakeRawStorage> {
  using pep::database::Storage<MakeRawStorage>::Storage;
};

void SqliteStorage::performAtomically(const Action& action) const {
  // Prevent reentrancy from other threads while allowing recursive calls (from the thread holding the lock)
  std::lock_guard lock(mutex_);

  // Determine if we're already in a transaction, i.e. this is a recursive invocation
  assert(transactionLevel_ != std::numeric_limits<decltype(transactionLevel_)>::max());
  auto initial = (transactionLevel_++ == 0U);
  PEP_DEFER(--transactionLevel_);

  if (initial) { // Not inside a transaction: start one now
    implementor_->transactionally(action);
  }
  else { // Already inside a transaction: don't start a new (nested) one
    action();
  }
}

std::optional<int64_t> SqliteStorage::getRowId(const std::string& name) const {
  using namespace sqlite_orm;
  return RangeToOptional(
    implementor_->raw.select(&RowRecord::seqno,
      where(c(&RowRecord::pseudonym) == name))); // Can produce a maximum of 1 RowRecord due to unique constraint
}

std::optional<int64_t> SqliteStorage::getColumnId(const std::string& name) const {
  using namespace sqlite_orm;
  return RangeToOptional(
    implementor_->raw.select(&ColumnRecord::seqno,
      where(c(&ColumnRecord::name) == name))); // Can produce a maximum of 1 ColumnRecord due to unique constraint
}

int64_t SqliteStorage::ensureRowId(const std::string& name) {
  using namespace sqlite_orm;
  auto result = this->getRowId(name);
  if (!result.has_value()) {
    result = implementor_->raw.insert(RowRecord{
      .pseudonym = name,
      });
  }
  return *result;
}

int64_t SqliteStorage::ensureColumnId(const std::string& name) {
  using namespace sqlite_orm;
  auto result = this->getColumnId(name);
  if (!result.has_value()) {
    result = implementor_->raw.insert(ColumnRecord{
      .name = name,
      });
  }
  return *result;
}

SqliteStorage::SqliteStorage(std::filesystem::path file)
  : implementor_(std::make_unique<Implementor>(file.string())) {
  implementor_->syncSchema();
}

SqliteStorage::~SqliteStorage() noexcept = default;

std::vector<RedactionCycle> SqliteStorage::getRedactionCycles() const {
  return this->atomically([this]() {
      // We can't pipe the result of raw.iterate<> directly into std::views::transform because sqlite_orm's mapped_view is not movable
      auto records = implementor_->raw.iterate<RedactionCycleRecord>(sqlite_orm::order_by(&RedactionCycleRecord::id).asc());
      return records
        | std::views::transform(&RecordToRedactionCycle)
        | std::ranges::to<std::vector>();
    });
}

RedactionCycle SqliteStorage::getRedactionCycle(RedactionCycle::Id id) const {
  return this->atomically([this, id]() {
      try {
        return RecordToRedactionCycle(implementor_->raw.get<RedactionCycleRecord>(id));
      }
      catch (const std::system_error& error) {
        auto condition = error.code().default_error_condition();
        if (condition.category() == sqlite_orm::get_orm_error_category() && condition.value() == std::to_underlying(sqlite_orm::orm_error_code::not_found)) {
          throw Error(std::format("Redaction cycle with id {} not found", id));
        }
        throw;
      }
    });
}

void SqliteStorage::updateRedactionCycle(const RedactionCycle& cycle) {
  return this->atomically([this, &cycle]() {
      implementor_->raw.update(RedactionCycleToRecord(cycle));
    });
}

RedactionCycle::Id SqliteStorage::createRedactionCycle(const RedactionCycle& cycle) {
  return this->atomically([this, &cycle]() {
      auto id = implementor_->raw.insert(RedactionCycleToRecord(cycle));

      try {
        return boost::numeric_cast<RedactionCycle::Id>(id);
      } catch (const boost::bad_numeric_cast&) {
        throw std::runtime_error("ID overflow in RedactionCycle table");
      }
    });
}

void SqliteStorage::deleteRedactionCycle(RedactionCycle::Id id) {
  return this->atomically([this, id]() {
      using namespace sqlite_orm;
      implementor_->raw.remove_all<RedactedEntryRecord>(where(c(&RedactedEntryRecord::cycle) == id));
      implementor_->raw.remove<RedactionCycleRecord>(id);
    });
}

void SqliteStorage::addToRedactionCycle(RedactionCycle::Id id, const CellVersionSpec& cellVersion) {
  return this->atomically([this, id, &cellVersion]() {
      try {
        implementor_->raw.insert(RedactedEntryRecord{
            .cycle = id,
            .row = this->ensureRowId(cellVersion.cell.participant()),
            .column = this->ensureColumnId(cellVersion.cell.column()),
            .timestamp = TicksSinceEpoch<std::chrono::milliseconds>(cellVersion.timestamp),
          });
      } catch (const std::system_error& error) {
        auto condition = error.code().default_error_condition();
        if (condition.category() == sqlite_orm::get_sqlite_error_category() && condition.value() == SQLITE_CONSTRAINT) {
          throw ConstraintViolation();
        }
        throw;
      }
    });
}

std::set<SqliteStorage::CellVersionSpec> SqliteStorage::getIncludedCellVersions(RedactionCycle::Id id, size_t skip, size_t max) const {
  return this->atomically([this, id, max, skip]() {
      using namespace sqlite_orm;
      return implementor_->raw.iterate(select(columns(&RowRecord::pseudonym, &ColumnRecord::name, &RedactedEntryRecord::timestamp),
        inner_join<RedactedEntryRecord>(on(c(&RedactedEntryRecord::row) == c(&RowRecord::seqno) && c(&RedactedEntryRecord::column) == c(&ColumnRecord::seqno))),
        where(c(&RedactedEntryRecord::cycle) == id),
        order_by(&RedactedEntryRecord::seqno), // Ensure deterministic order so that...
        limit(max, offset(skip)))) // ... limit and offset produce consistent results
        | std::views::transform([](auto tuple) {
            auto& [participant, column, dbMillis] = tuple;
            return CellVersionSpec{
                .cell = EntryName(std::move(participant), std::move(column)),
                .timestamp = database::TimestampFromUnixMillis(dbMillis),
            };
          })
        | std::ranges::to<std::set>();
    });
}

}
