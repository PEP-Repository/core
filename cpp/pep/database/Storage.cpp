#include <pep/database/Storage.hpp>

namespace {
std::string generateSchemaErrorMessage(std::string_view table, pep::database::SchemaError::Reason reason) {
  using namespace pep::database;
  switch (reason) {
  case SchemaError::Reason::dropped_and_recreated:
    return std::format("Schema synchronization for table {} will drop and recreate the table, resulting in data loss", table);
  case SchemaError::Reason::old_columns_removed:
    return std::format("Schema synchronization for table {} will remove old columns", table);
  }
}
}

namespace pep::database {

SchemaError::SchemaError(std::string table, Reason reason)
    : logic_error(generateSchemaErrorMessage(table, reason)), mTable(std::move(table)), mReason(reason) {}

const char* const BasicStorage::STORE_IN_MEMORY = ":memory:";

BasicStorage::BasicStorage(const std::string& path)
  : isPersistent(path != STORE_IN_MEMORY) {
}

}
