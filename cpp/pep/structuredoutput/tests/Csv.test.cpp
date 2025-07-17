#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Csv.hpp>

namespace {
namespace csv = pep::structuredOutput::csv;
using pep::structuredOutput::Table;


TEST(structuredOutputCsv, FromEmptyTableWithHeader) {
  const auto emptyTable = Table::FromSeparateHeaderAndData({"name", "value 1", "value 2"}, {});

  EXPECT_EQ(csv::to_string(emptyTable), "\"name\";\"value 1\";\"value 2\"\n");
}

TEST(structuredOutputCsv, FromPopulatedTableWithHeader) {
  const auto table = Table::FromSeparateHeaderAndData({"name", "value 1", "value 2"}, {"id001", "1.45", "3", "id002", "1.55", "4"});

  EXPECT_EQ(
      csv::to_string(table),
      "\"name\";\"value 1\";\"value 2\"\n"
      "\"id001\";\"1.45\";\"3\"\n"
      "\"id002\";\"1.55\";\"4\"\n");
}

TEST(structuredOutputCsv, UsesTheDelimiterFromTheConfig) {
  const auto table = Table::FromSeparateHeaderAndData({"H1", "H2"}, {"R1", "R2"});

  EXPECT_EQ(
      csv::to_string(table, {.delimiter = csv::Delimiter::comma}),
      "\"H1\",\"H2\"\n"
      "\"R1\",\"R2\"\n");
  EXPECT_EQ(
      csv::to_string(table, {.delimiter = csv::Delimiter::semicolon}),
      "\"H1\";\"H2\"\n"
      "\"R1\";\"R2\"\n");
  EXPECT_EQ(
      csv::to_string(table, {.delimiter = csv::Delimiter::tab}),
      "\"H1\"\t\"H2\"\n"
      "\"R1\"\t\"R2\"\n");
}

TEST(structuredOutputCsv, DoubleQuotesInRecordFieldsAreEscaped) {
  const auto table = Table::EmptyWithHeader({"...\"a\"\"b\"..."});

  EXPECT_EQ(csv::to_string(table), "\"...\"\"a\"\"\"\"b\"\"...\"\n");
}

TEST(structuredOutputCsv, DoubleQuotesInHeaderFieldsAreEscaped) {
  const auto table = Table::FromSeparateHeaderAndData({"...\"...\"\"..."}, {});

  EXPECT_EQ(csv::to_string(table), "\"...\"\"...\"\"\"\"...\"\n");
}

}
