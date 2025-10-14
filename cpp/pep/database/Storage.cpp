#include <pep/database/Storage.hpp>

namespace {
std::string generateSchemaErrorMessage(std::string_view table, sqlite_orm::sync_schema_result result) {
    std::ostringstream oss;
    switch (result) {
    case sqlite_orm::sync_schema_result::dropped_and_recreated:
      oss << "Schema synchronization for table " << table << " will drop and recreate the table, resulting in data loss";
      break;
    case sqlite_orm::sync_schema_result::old_columns_removed:
    case sqlite_orm::sync_schema_result::new_columns_added_and_old_columns_removed:
      oss << "Schema synchronization for table " << table << " will remove old columns";
      break;
    default:
      oss << "Error during database schema synchronization for table " <<  table << ": " << result;
    }
    return oss.str();
}
}

namespace pep::database {

SchemaError::SchemaError(std::string table, sqlite_orm::sync_schema_result result)
    : logic_error(generateSchemaErrorMessage(table, result)), mTable(std::move(table)), mResult(result) {}

const char* const BasicStorage::STORE_IN_MEMORY = ":memory:";

BasicStorage::BasicStorage(const std::string& path)
  : isPersistent(path != STORE_IN_MEMORY) {
}

}
