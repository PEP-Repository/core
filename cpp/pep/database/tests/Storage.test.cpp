#include <gtest/gtest.h>
#include <sqlite_orm/sqlite_orm.h>
#include <pep/database/Storage.hpp>
#include <pep/utils/Filesystem.hpp>

namespace {
using namespace sqlite_orm;

struct MyTableRecord {
  int id;
  std::string key;
  std::string value;
};

struct ExtraTableRecord {
  int id;
  std::string value;
};

auto MakeStorageBase(const std::string& path) {
  return make_storage(path,
    make_table("MyTable",
      make_column("id", &MyTableRecord::id, primary_key()),
      make_column("key", &MyTableRecord::key)));
}

auto MakeStorageWithExtraColumn(const std::string& path) {
  return make_storage(path,
    make_table("MyTable",
      make_column("id", &MyTableRecord::id, primary_key()),
      make_column("key", &MyTableRecord::key),
      make_column("value", &MyTableRecord::value)));
}

auto MakeStorageWithExtraColumnWithDefaultValue(const std::string& path) {
  return make_storage(path,
    make_table("MyTable",
      make_column("id", &MyTableRecord::id, primary_key()),
      make_column("key", &MyTableRecord::key),
      make_column("value", &MyTableRecord::value, default_value(""))));
}

auto MakeStorageWithExtraTable(const std::string& path) {
  return make_storage(path,
    make_table("MyTable",
      make_column("id", &MyTableRecord::id, primary_key()),
      make_column("key", &MyTableRecord::key)),
    make_table("ExtraTable",
      make_column("id", &ExtraTableRecord::id, primary_key()),
      make_column("value", &ExtraTableRecord::value)));
}
}

class StorageTest : public ::testing::Test {
public:
  static pep::filesystem::Temporary Tempdir;

  static void SetUpTestSuite() {
    namespace fs = pep::filesystem;
    Tempdir = fs::Temporary{fs::temp_directory_path() / fs::RandomizedName("pepTest-Database-Storage-%%%%-%%%%-%%%%")};
    fs::create_directory(Tempdir.path());
  }

  static std::filesystem::path getDbPath() {
    return Tempdir.path() / (std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + ".sqlite");
  }
};
pep::filesystem::Temporary StorageTest::Tempdir;


TEST_F(StorageTest, syncSchema_returns_whether_changes_have_been_made) {
  std::filesystem::path dbPath = getDbPath();
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_FALSE(storage.syncSchema());
  }
}

TEST_F(StorageTest, syncSchema_with_new_column_fails_without_default_value) {
  std::filesystem::path dbPath = getDbPath();
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
  {
    pep::database::Storage<MakeStorageWithExtraColumn> storage(dbPath);
    try {
      storage.syncSchema();
      FAIL() << "syncShema should have thrown";
    }
    catch (pep::database::SchemaError& e) {
      EXPECT_EQ(e.mTable, "MyTable");
      EXPECT_EQ(e.mResult, sqlite_orm::sync_schema_result::dropped_and_recreated);
    }
  }
}

TEST_F(StorageTest, syncSchema_with_new_column_succeeds_with_default_value) {
  std::filesystem::path dbPath = getDbPath();
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
  {
    pep::database::Storage<MakeStorageWithExtraColumnWithDefaultValue> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
}

TEST_F(StorageTest, syncSchema_with_removed_column_depends_on_parameter) {
  std::filesystem::path dbPath = getDbPath();
  {
    pep::database::Storage<MakeStorageWithExtraColumn> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    try {
      storage.syncSchema();
      FAIL() << "syncShema should have thrown";
    }
    catch (pep::database::SchemaError& e) {
      EXPECT_EQ(e.mTable, "MyTable");
      EXPECT_EQ(e.mResult, sqlite_orm::sync_schema_result::old_columns_removed);
    }
  }
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema(true));
  }
}

TEST_F(StorageTest, syncSchema_with_new_table_succeeds) {
  std::filesystem::path dbPath = getDbPath();
  {
    pep::database::Storage<MakeStorageBase> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
  {
    pep::database::Storage<MakeStorageWithExtraTable> storage(dbPath);
    EXPECT_TRUE(storage.syncSchema());
  }
}

