#include <pep/database/Storage.hpp>
#include <pep/database/UnixMillis.hpp>
#include <pep/storagefacility/PurgeMessages.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <sqlite_orm/sqlite_orm.h>

namespace pep {

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
      make_column("announced", &PurgeCycleRecord::announced)),
    make_table("PurgeEntry",
      make_column("cycle", &PurgeEntryRecord::cycle),
      make_column("row", &PurgeEntryRecord::row),
      make_column("column", &PurgeEntryRecord::column),
      make_column("timestamp", &PurgeEntryRecord::timestamp))
  );
}

PurgeCycle RecordToPurgeCycle(PurgeCycleRecord record) {
  return PurgeCycle{
    .id = record.id,
    .definition = PurgeCycle::Definition(std::move(record.name)),
    .announced = database::TimestampFromUnixMillis(record.announced),
  };
}

} // End anonymous namespace

struct StorageFacility::SqliteStorage::Implementor : public pep::database::Storage<MakeRawStorage> {
  using pep::database::Storage<MakeRawStorage>::Storage;
};

int64_t StorageFacility::SqliteStorage::getRowId(const std::string& name) {
  using namespace sqlite_orm;
  auto result = RangeToOptional(
    implementor_->raw.select(&RowRecord::seqno,
      where(c(&RowRecord::pseudonym) == name),
      limit(1)));
  if (!result.has_value()) {
    result = implementor_->raw.insert(RowRecord{
      .pseudonym = name,
      });
  }
  return *result;
}

int64_t StorageFacility::SqliteStorage::getColumnId(const std::string& name) {
  using namespace sqlite_orm;
  auto result = RangeToOptional(
    implementor_->raw.select(&ColumnRecord::seqno,
      where(c(&ColumnRecord::name) == name),
      limit(1)));
  if (!result.has_value()) {
    result = implementor_->raw.insert(ColumnRecord{
      .name = name,
      });
  }
  return *result;
}

void StorageFacility::SqliteStorage::deletePurgeCycleRecord(int64_t id) {
  implementor_->raw.remove<PurgeCycleRecord>(id);
}

StorageFacility::SqliteStorage::SqliteStorage(std::filesystem::path file)
  : implementor_(std::make_unique<Implementor>(file.string())) {
  implementor_->syncSchema();
}

StorageFacility::SqliteStorage::~SqliteStorage() noexcept = default;

std::vector<PurgeCycle> StorageFacility::SqliteStorage::getPurgeCycles() const {
  return RangeToVector(implementor_->raw.get_all<PurgeCycleRecord>()
    | std::ranges::views::transform(&RecordToPurgeCycle));
}

PurgeCycle StorageFacility::SqliteStorage::getPurgeCycle(PurgeCycle::Id id) const {
  return RecordToPurgeCycle(implementor_->raw.get<PurgeCycleRecord>(id));
}

PurgeCycle::Id StorageFacility::SqliteStorage::createPurgeCycle(PurgeCycle::Definition definition) {
  auto id = implementor_->raw.insert(PurgeCycleRecord{
    .name = std::move(definition).name(),
    .announced = database::TimestampToUnixMillis(TimeNow()),
    });

  PurgeCycle::Id result;
  try {
    result = boost::numeric_cast<PurgeCycle::Id>(id);
  }
  catch (...) {
    this->deletePurgeCycleRecord(id);
    throw std::runtime_error("ID overflow in PurgeCycle table");
  }
  return result;
}

void StorageFacility::SqliteStorage::deletePurgeCycle(PurgeCycle::Id id) {
  using namespace sqlite_orm;
  implementor_->raw.remove_all<PurgeEntryRecord>(where(c(&PurgeEntryRecord::cycle) == id));
  this->deletePurgeCycleRecord(id);
}

void StorageFacility::SqliteStorage::addToPurgeCycle(PurgeCycle::Id id, const std::string& row, const std::string& column, Timestamp timestamp) {
  implementor_->raw.insert(PurgeEntryRecord{
    .cycle = id,
    .row = this->getRowId(row),
    .column = this->getColumnId(column),
    .timestamp = TicksSinceEpoch<std::chrono::milliseconds>(timestamp),
    });
}

}
