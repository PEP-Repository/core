#include <pep/database/Storage.hpp>
#include <pep/database/UnixMillis.hpp>
#include <pep/storagefacility/PurgeMessages.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <pep/utils/Defer.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <sqlite_orm/sqlite_orm.h>

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

struct PurgeCycleRecord {
  PurgeCycle::Id id{};
  std::string name;
  database::UnixMillis announced;
  database::UnixMillis confirmed{};
  database::UnixMillis completed{};
};

struct PurgeEntryRecord {
  PurgeCycle::Id cycle; // FK to PurgeCycleRecord::id
  int64_t row;        // FK to RowRecord::seqno
  int64_t column;     // FK to ColumnRecord::seqno
  database::UnixMillis timestamp;
};

auto MakeRawStorage(std::string path) {
  using namespace sqlite_orm;
  return make_storage(std::move(path),
    make_table("Row",
      make_column("seqno", &RowRecord::seqno,
        primary_key().autoincrement()),
      make_column("pseudonym", &RowRecord::pseudonym)),

    make_table("Column",
      make_column("seqno", &ColumnRecord::seqno,
        primary_key().autoincrement()),
      make_column("name", &ColumnRecord::name)),

    make_table("PurgeCycle",
      make_column("id", &PurgeCycleRecord::id,
        primary_key().autoincrement()),
      make_column("name", &PurgeCycleRecord::name),
      make_column("announced", &PurgeCycleRecord::announced),
      make_column("confirmed", &PurgeCycleRecord::confirmed),
      make_column("completed", &PurgeCycleRecord::completed)),

    make_unique_index("idx_PurgeEntry",
      &PurgeEntryRecord::row,
      &PurgeEntryRecord::column,
      &PurgeEntryRecord::timestamp),
    make_table("PurgeEntry",
      make_column("cycle", &PurgeEntryRecord::cycle),
      make_column("row", &PurgeEntryRecord::row),
      make_column("column", &PurgeEntryRecord::column),
      make_column("timestamp", &PurgeEntryRecord::timestamp))
  );
}

std::optional<Timestamp> OptionalTimestampFromUnixMillis(database::UnixMillis millis) {
  if (millis != 0) {
    return database::TimestampFromUnixMillis(millis);
  }
  return std::nullopt;
}

database::UnixMillis OptionalTimestampToUnixMillis(const std::optional<Timestamp>& timestamp) {
  return database::TimestampToUnixMillis(timestamp.value_or(Timestamp{ /* zero */}));
}

PurgeCycle RecordToPurgeCycle(PurgeCycleRecord record) {
  return PurgeCycle{
    .id = record.id,
    .definition = PurgeCycle::Definition(std::move(record.name)),
    .announced = database::TimestampFromUnixMillis(record.announced),
    .confirmed = OptionalTimestampFromUnixMillis(record.confirmed),
    .completed = OptionalTimestampFromUnixMillis(record.completed),
  };
}

PurgeCycleRecord PurgeCycleToRecord(const PurgeCycle& cycle) {
  return PurgeCycleRecord{
    .id = cycle.id,
    .name = cycle.definition.name(),
    .announced = database::TimestampToUnixMillis(cycle.announced),
    .confirmed = OptionalTimestampToUnixMillis(cycle.confirmed),
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
  // Ensure thread safe
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  // Determine if we're already in a transaction (i.e. this is a reentrant invocation)
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
      where(c(&RowRecord::pseudonym) == name),
      limit(1)));
}

std::optional<int64_t> SqliteStorage::getColumnId(const std::string& name) const {
  using namespace sqlite_orm;
  return RangeToOptional(
    implementor_->raw.select(&ColumnRecord::seqno,
      where(c(&ColumnRecord::name) == name),
      limit(1)));
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

void SqliteStorage::deletePurgeCycleRecord(int64_t id) {
  implementor_->raw.remove<PurgeCycleRecord>(id);
}

SqliteStorage::SqliteStorage(std::filesystem::path file)
  : implementor_(std::make_unique<Implementor>(file.string())) {
  implementor_->syncSchema();
}

SqliteStorage::~SqliteStorage() noexcept = default;

std::vector<PurgeCycle> SqliteStorage::getPurgeCycles() const {
  return this->atomically([this]() {
      return RangeToVector(
        RangeToVector(implementor_->raw.iterate<PurgeCycleRecord>(sqlite_orm::order_by(&PurgeCycleRecord::id).asc()))
        | std::ranges::views::transform(&RecordToPurgeCycle)
      );
    });
}

PurgeCycle SqliteStorage::getPurgeCycle(PurgeCycle::Id id) const {
  return this->atomically([this, id]() {
      return RecordToPurgeCycle(implementor_->raw.get<PurgeCycleRecord>(id));
    });
}

void SqliteStorage::updatePurgeCycle(const PurgeCycle& cycle) {
  return this->atomically([this, &cycle]() {
      implementor_->raw.update(PurgeCycleToRecord(cycle));
    });
}

PurgeCycle::Id SqliteStorage::createPurgeCycle(const PurgeCycle& cycle) {
  return this->atomically([this, &cycle]() {
      auto id = implementor_->raw.insert(PurgeCycleToRecord(cycle));

      try {
        return boost::numeric_cast<PurgeCycle::Id>(id);
      } catch (...) {
        throw std::runtime_error("ID overflow in PurgeCycle table");
      }
    });
}

void SqliteStorage::deletePurgeCycle(PurgeCycle::Id id) {
  return this->atomically([this, id]() {
      using namespace sqlite_orm;
      implementor_->raw.remove_all<PurgeEntryRecord>(where(c(&PurgeEntryRecord::cycle) == id));
      this->deletePurgeCycleRecord(id);
    });
}

void SqliteStorage::addToPurgeCycle(PurgeCycle::Id id, const CellVersionSpec& cellVersion) {
  return this->atomically([this, id, &cellVersion]() {
      try {
        implementor_->raw.insert(PurgeEntryRecord{
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

std::set<SqliteStorage::CellVersionSpec> SqliteStorage::getIncludedCellVersions(PurgeCycle::Id id) const {
  return this->atomically([this, id]() {
      std::set<CellVersionSpec> result;

      using namespace sqlite_orm;
      for (auto entry : implementor_->raw.select(columns(&RowRecord::pseudonym, &ColumnRecord::name, &PurgeEntryRecord::timestamp),
        where(c(&PurgeEntryRecord::cycle) == id
          && c(&PurgeEntryRecord::row) == c(&RowRecord::seqno)
          && c(&PurgeEntryRecord::column) == c(&ColumnRecord::seqno)))) {
        const auto& participant = std::get<0>(entry);
        const auto& column = std::get<1>(entry);
        result.emplace(CellVersionSpec{
            .cell = EntryName(participant, column),
            .timestamp = database::TimestampFromUnixMillis(std::get<2>(entry)),
          });
      }
      return result;
    });
}

std::set<Timestamp> SqliteStorage::getPurgedTimestamps(PurgeCycle::Id id, const EntryName& cell) const {
  return this->atomically([this, id, &cell]() {
      std::set<Timestamp> result;

      using namespace sqlite_orm;
      auto rowId = this->getRowId(cell.participant());
      auto columnId = this->getColumnId(cell.column());

      if (rowId.has_value() && columnId.has_value()) {
        for (auto millis : implementor_->raw.select(&PurgeEntryRecord::timestamp,
          where(c(&PurgeEntryRecord::cycle) == id
            && c(&PurgeEntryRecord::row) == *rowId
            && c(&PurgeEntryRecord::column) == *columnId),
          order_by(&PurgeEntryRecord::timestamp))) {
          [[maybe_unused]] auto emplaced = result.emplace(database::TimestampFromUnixMillis(millis)).second;
          assert(emplaced);
        }
      }

      return result;
    });
}

std::optional<Timestamp> SqliteStorage::pendingPurgeTimestamp(const CellVersionSpec& cellVersion) const {
  return this->atomically([this, &cellVersion]() -> std::optional<Timestamp> {
      auto rowId = this->getRowId(cellVersion.cell.participant());
      auto columnId = this->getColumnId(cellVersion.cell.column());
      if (!rowId.has_value() || !columnId.has_value()) {
        return std::nullopt;
      }

      using namespace sqlite_orm;
      auto millis = database::TimestampToUnixMillis(cellVersion.timestamp);

      auto confirmed = implementor_->raw.select(&PurgeCycleRecord::confirmed,
        inner_join<PurgeEntryRecord>(on(c(&PurgeEntryRecord::cycle) == c(&PurgeCycleRecord::id))),
        where(c(&PurgeEntryRecord::row) == rowId && c(&PurgeEntryRecord::column) == columnId && c(&PurgeEntryRecord::timestamp) == millis
          && c(&PurgeCycleRecord::confirmed) != 0 && c(&PurgeCycleRecord::completed) == 0));
      if (confirmed.empty()) {
        return std::nullopt;
      }
      assert(confirmed.size() == 1U);
      return database::TimestampFromUnixMillis(confirmed.front());
    });
}

}
