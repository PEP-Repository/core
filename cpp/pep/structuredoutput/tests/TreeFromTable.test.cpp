#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Tree.hpp>

namespace {
using nlohmann::json;
using pep::structuredOutput::Table;
using pep::structuredOutput::Tree;

nlohmann::json At(const Tree& t, std::string_view k0) { return t.toJson().at(k0); }
nlohmann::json At(const Tree& t, std::string_view k0, std::string_view k1) { return t.toJson().at(k0).at(k1); }

} // namespace

TEST(structuredOutputTreeFromTable, converts_tables_to_arrays_of_objects) {
  { // case: empty table
    const auto table = Table::EmptyWithHeader({"column A", "column B"});

    const auto tree = TreeFromTable(table);

    EXPECT_EQ(At(tree, "data"), json::array());
  }

  { // case: populated table
    const auto table = Table::FromSeparateHeaderAndData({"fruit", "color"}, {"apple", "red", "pear", "green"});

    const auto tree = TreeFromTable(table);

    EXPECT_EQ(
        At(tree, "data"),
        json::array({{{"fruit", "apple"}, {"color", "red"}}, {{"fruit", "pear"}, {"color", "green"}}}));
  }
}

TEST(structuredOutputTreeFromTable, includes_the_table_header_as_metadata) {
  const auto table = Table::EmptyWithHeader({"column A", "column B"});

  const auto tree = TreeFromTable(table);

  EXPECT_EQ(At(tree, "metadata", "header"), json::array({"column A", "column B"}));
}
