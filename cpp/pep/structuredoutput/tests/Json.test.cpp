#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Json.hpp>

#include <sstream>

namespace {
namespace json_out = pep::structuredOutput::json;
using pep::structuredOutput::JsonConfig;
using pep::structuredOutput::Table;
using pep::structuredOutput::Tree;
using pep::structuredOutput::WhitespaceFormat;

/// Small helper so that the tests can read like the other to_string based tests in this directory
template <typename T>
std::string ToString(const T& subject, const JsonConfig& config = {}) {
  std::ostringstream stream;
  json_out::append(stream, subject, config);
  return std::move(stream).str();
}

TEST(structuredOutputJson, KeysAppearAsConstructed) {
  EXPECT_EQ(
      ToString(Tree::FromJson({{"C", nullptr}, {"D", nullptr}, {"B", nullptr}, {"A", nullptr}}), {.wsFormat = WhitespaceFormat::Compact}),
               R"({"C":null,"D":null,"B":null,"A":null})") << "keys should not be sorted";
}

TEST(structuredOutputJson, WhitespaceFormat) {
  const auto tree = Tree::FromJson({{"a", 1}, {"b", {1, 2}}});

  EXPECT_EQ(ToString(tree, {.wsFormat = WhitespaceFormat::Compact}),
            R"({"a":1,"b":[1,2]})");

  EXPECT_EQ(ToString(tree, {.wsFormat = WhitespaceFormat::TwoSpaces}),
            "{\n"
            "  \"a\": 1,\n"
            "  \"b\": [\n"
            "    1,\n"
            "    2\n"
            "  ]\n"
            "}");

  EXPECT_EQ(ToString(tree, {.wsFormat = WhitespaceFormat::FourSpaces}),
            "{\n"
            "    \"a\": 1,\n"
            "    \"b\": [\n"
            "        1,\n"
            "        2\n"
            "    ]\n"
            "}");

  EXPECT_EQ(ToString(tree),
            ToString(tree, {.wsFormat = WhitespaceFormat::TwoSpaces})) << "two spaces is the default";
}

TEST(structuredOutputJson, FromPopulatedTableWithHeader) {
  const auto table = Table::FromSeparateHeaderAndData({"fruit", "color"}, {"apple", "red", "pear", "green"});

  std::ostringstream stream;
  json_out::append(stream, table, {.wsFormat = WhitespaceFormat::Compact}) << '\n'; // chaining needs the same stream back

  EXPECT_EQ(std::move(stream).str(),
            R"({"metadata":{"header":["fruit","color"]},)"
            R"("data":[{"fruit":"apple","color":"red"},{"fruit":"pear","color":"green"}]})"
            "\n");
}

} // namespace
