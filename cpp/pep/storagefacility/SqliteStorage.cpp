#include <pep/database/Storage.hpp>
#include <pep/storagefacility/PurgeMessages.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <sqlite_orm/sqlite_orm.h>

namespace pep {

namespace {

// Normalized storage for row pseudonyms to reduce storage space use
struct RowRecord {
  int64_t seqno;
  std::string pseudonym; // text representation of the row's local pseudonym for SF
};

// Normalized storage for column names to reduce storage space use
struct ColumnRecord {
  int64_t seqno;
  std::string name;
};

struct PurgeCycleRecord {
  PurgeCycleId id;
  std::string name;
};

struct PurgeEntryRecord {
  PurgeCycleId cycle; // FK to PurgeCycleRecord::id
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
      make_column("name", &PurgeCycleRecord::name)),
    make_table("PurgeEntry",
      make_column("cycle", &PurgeEntryRecord::cycle),
      make_column("row", &PurgeEntryRecord::row),
      make_column("column", &PurgeEntryRecord::column),
      make_column("timestamp", &PurgeEntryRecord::timestamp))
  );
}

} // End anonymous namespace

struct StorageFacility::SqliteStorage::Implementor : public pep::database::Storage<MakeRawStorage> {
  using pep::database::Storage<MakeRawStorage>::Storage;
};

StorageFacility::SqliteStorage::SqliteStorage(std::filesystem::path file)
  : implementor_(std::make_unique<Implementor>(file.string())) {
}

StorageFacility::SqliteStorage::~SqliteStorage() noexcept = default;

PurgeCycleId StorageFacility::SqliteStorage::startPurgeCycle(PurgeCycleDefinition definition) {
  // TODO: validate against existing DB
  return implementor_->raw.insert(PurgeCycleRecord{
    .name = std::move(definition).name(),
    });
  // TODO: implementor_->raw.insert(PurgeEntryRecord...)
}

void StorageFacility::SqliteStorage::cancelPurge(PurgeCycleId id) {
  implementor_->raw.remove<PurgeCycleRecord>(id);
  // TODO: implementor_->raw.remove(PurgeEntryRecord...)
}

}
