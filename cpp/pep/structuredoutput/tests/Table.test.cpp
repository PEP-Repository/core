#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Table.hpp>
#include <stdexcept>
#include <vector>

namespace {
using pep::structuredOutput::Table;
using testing::ElementsAre;

TEST(structuredOutputTable, EmptyWithHeader_isEmpty) {
  const auto table = Table::EmptyWithHeader({"arbitraryHeader"});
  EXPECT_EQ(table.size(), 0);
}

TEST(structuredOutputTable, EmptyWithHeader_retainsHeaderPassedAtCreation) {
  const auto table = Table::EmptyWithHeader({"columnA", "columnB"});
  EXPECT_THAT(table.header(), ElementsAre("columnA", "columnB"));
}

TEST(structuredOutputTable, EmptyWithHeader_setsTheRecordSize) {
  EXPECT_EQ(Table::EmptyWithHeader({"1", "2", "3", "4"}).recordSize(), 4);
  EXPECT_EQ(Table::EmptyWithHeader({"1", "2", "3"}).recordSize(), 3);
}

TEST(structuredOutputTable, EmptyWithHeader_throwsIfHeaderIsEmpty) {
  EXPECT_THROW(Table::EmptyWithHeader({}), std::runtime_error);
}

TEST(structuredOutputTable, FromSeparateHeaderAndData_retainsDataPassedAtCreation) {
  const auto arbitraryColumnNames = std::vector<std::string>{"id", "name", "number"};

  { // Case: single record
    const auto singleRecordTable = Table::FromSeparateHeaderAndData(arbitraryColumnNames, {"id0001", "only", "120"});
    const auto records = singleRecordTable.records();

    ASSERT_EQ(records.size(), 1);
    EXPECT_THAT(records.front(), ElementsAre("id0001", "only", "120"));
  }

  { // Case: multiple records
    const auto multiRecordTable = Table::FromSeparateHeaderAndData(
        arbitraryColumnNames,
        {"id0001", "first", "335", "id0002", "second", "53", "id0003", "third", "2"});
    const auto records = multiRecordTable.records();

    ASSERT_EQ(records.size(), 3);
    EXPECT_THAT(records[0], ElementsAre("id0001", "first", "335"));
    EXPECT_THAT(records[1], ElementsAre("id0002", "second", "53"));
    EXPECT_THAT(records[2], ElementsAre("id0003", "third", "2"));
  }

  { // Case: zero records
    const auto multiRecordTable = Table::FromSeparateHeaderAndData(arbitraryColumnNames, {});
    const auto records = multiRecordTable.records();

    EXPECT_TRUE(records.empty());
  }
}

TEST(structuredOutputTable, FromSeparateHeaderAndData_setsTheRecordSize) {
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1"}, {"some", "data", "fields"}).recordSize(), 1);
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1", "2", "3"}, {"some", "data", "fields"}).recordSize(), 3);
}

TEST(structuredOutputTable, FromSeparateHeaderAndData_throwsIfHeaderIsEmpty) {
  EXPECT_THROW(Table::FromSeparateHeaderAndData({}, {}), std::runtime_error);
  EXPECT_THROW(Table::FromSeparateHeaderAndData({}, {"arbitrary", "data"}), std::runtime_error);
}

TEST(structuredOutputTable, FromSeparateHeaderAndData_throwsIfDataSizeDoesNotMatchTokenSize) {
  EXPECT_THROW(Table::FromSeparateHeaderAndData({"1", "2"}, {"a", "b", "c"}), std::runtime_error);
}

TEST(structuredOutputTable, SizeIsEqualToTheNumberOfRecords) {
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1", "2"}, {}).size(), 0);
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1", "2", "3"}, {"a", "a", "a"}).size(), 1);
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1", "2"}, {"a", "a", "b", "b", "c", "c"}).size(), 3);
  EXPECT_EQ(Table::FromSeparateHeaderAndData({"1"}, {"a", "b", "c", "d"}).size(), 4);
}

TEST(structuredOutputTable, EmptyIsTrueIfThereAreNoRecords) {
  EXPECT_TRUE(Table::FromSeparateHeaderAndData({"1", "2"}, {}).empty());
}

TEST(structuredOutputTable, EmptyIsFalseIfThereAreOneOrMoreRecords) {
  EXPECT_FALSE(Table::FromSeparateHeaderAndData({"1"}, {"a"}).empty());
  EXPECT_FALSE(Table::FromSeparateHeaderAndData({"1"}, {"a", "b"}).empty());
}

TEST(structuredOutputTable, ReserveIncreasesCapacityIfNeeded) {
  auto table = Table::EmptyWithHeader({"header"});

  const auto initialCapacity = table.capacity();
  table.reserve(initialCapacity + 1);
  EXPECT_GE(table.capacity(), initialCapacity + 1);

  const auto capacityAfterFirstReserve = table.capacity();
  table.reserve(capacityAfterFirstReserve * 4);
  EXPECT_GE(table.capacity(), capacityAfterFirstReserve * 4);
}

TEST(structuredOutputTable, EmplaceBack_AddsARecordAtTheEnd) {
  auto table = Table::EmptyWithHeader({"fruit", "color"});

  table.emplace_back({"banana", "yellow"});
  EXPECT_EQ(table.size(), 1);
  EXPECT_THAT(table.records().back(), ElementsAre("banana", "yellow"));

  table.emplace_back({"apple", "green"});
  EXPECT_EQ(table.size(), 2);
  EXPECT_THAT(table.records().back(), ElementsAre("apple", "green"));
}

TEST(structuredOutputTable, EmplaceBack_ReturnsTheCreatedRecord) {
  auto table = Table::EmptyWithHeader({"fruit", "color"});

  Table::ConstRecordRef returnedRecordRef = table.emplace_back({"pear", "#d1e231"});
  Table::ConstRecordRef lastRecordInTable = table.records().back();

  EXPECT_EQ(std::addressof(returnedRecordRef.front()), std::addressof(lastRecordInTable.front()));
}

TEST(structuredOutputTable, EmplaceBack_ThrowsIfTheNumberOfFieldsDoesNotMatchTheRecordSize) {
  auto twoColumnTable = Table::EmptyWithHeader({"fruit", "color"});
  EXPECT_THROW(twoColumnTable.emplace_back({"banana", "yellow", "green"});, std::runtime_error);
  EXPECT_THROW(twoColumnTable.emplace_back({"banana"});, std::runtime_error);
}

TEST(structuredOutputTable, EmplaceBack_FieldsCanBeOverwritten) {
  auto table = Table::EmptyWithHeader({"fruit", "color"});

  auto emplaced = table.emplace_back({"?", "red"});
  emplaced[0] = "strawberry";

  EXPECT_THAT(table.records().front(), ElementsAre("strawberry", "red"));
}

TEST(structuredOutputTable, Records_FieldsCanBeOverwritten) {
  auto table = Table::FromSeparateHeaderAndData({"fruit", "color"}, {"apple", "?", "banana", "?"});

  table.records()[0][1] = "green";
  table.records()[1][1] = "yellow";

  EXPECT_THAT(table.records().front(), ElementsAre("apple", "green"));
  EXPECT_THAT(table.records().back(), ElementsAre("banana", "yellow"));
}

TEST(structuredOutputTable, Header_FieldsCanBeOverwritten) {
  auto table = Table::FromSeparateHeaderAndData({"fruit", "???"}, {"apple", "green", "banana", "yellow"});

  table.header()[1] = "color";

  EXPECT_THAT(table.header(), ElementsAre("fruit", "color"));
}

TEST(structuredOutputTable, ForEachFieldInColumn) {
  auto table = Table::FromSeparateHeaderAndData({"notCleared", "cleared"}, {"a", "b", "c", "d"});
  const auto makeEmpty = [](std::string& s) { return s.clear(); };

  pep::structuredOutput::ForEachFieldInColumn(table, 1, makeEmpty);

  EXPECT_FALSE(table.records()[0][0].empty() || table.records()[1][0].empty());
  EXPECT_TRUE(table.records()[0][1].empty() && table.records()[1][1].empty());
}

TEST(structuredOutputTable, AllOfFieldsInColumn) {
  const auto table = Table::FromSeparateHeaderAndData({"allEmpty", "notAllEmpty"}, {"", "", "", "value"});
  const auto isEmpty = [](const std::string& s) { return s.empty(); };

  EXPECT_TRUE(AllOfFieldsInColumn(table, 0, isEmpty));
  EXPECT_FALSE(AllOfFieldsInColumn(table, 1, isEmpty));
}

}
