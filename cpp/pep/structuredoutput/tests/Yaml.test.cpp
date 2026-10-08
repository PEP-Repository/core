#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Yaml.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <sstream>

namespace {
namespace yaml = pep::structuredOutput::yaml;
using pep::structuredOutput::Table;
using pep::structuredOutput::Tree;
using json = nlohmann::ordered_json;

TEST(structuredOutputYaml, NakedValues) {
  // naked values
  EXPECT_EQ(yaml::to_string(Tree::FromJson("this string")), "\"this string\"\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(false)), "false\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(52)), "52\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(nullptr)), "null\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(json::object())), "{}\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(json::array())), "[]\n");
}

TEST(structuredOutputYaml, StringLiteralEscaping) {
  EXPECT_EQ(
      yaml::to_string(Tree::FromJson({{"unquoted0", "always quote rhs"}, {"unquoted _key_", "escape \\ and \"!"}})),
      "unquoted0: \"always quote rhs\"\n"
      "unquoted _key_: \"escape \\\\ and \\\"!\"\n");
  EXPECT_EQ(
      yaml::to_string(Tree::FromJson({{"0key", "quote keys that do not start with alpha"}, {"k|:", "special chars"}})),
      "\"0key\": \"quote keys that do not start with alpha\"\n"
      "\"k|:\": \"special chars\"\n");
}

TEST(structuredOutputYaml, FlatArray) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson({"simple string", true, 17, nullptr, json::object(), json::array()})),
      "- \"simple string\"\n"
      "- true\n"
      "- 17\n"
      "- null\n"
      "- {}\n"
      "- []\n");
}

TEST(structuredOutputYaml, FlatMap) {
  EXPECT_EQ(
      yaml::to_string(Tree::FromJson({{"C", nullptr}, {"D", nullptr}, {"B", nullptr}, {"A", nullptr}})),
      "C: null\n"
      "D: null\n"
      "B: null\n"
      "A: null\n")
      << "keys should appear as constructed";

  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{"key 1", "string"},
               {"key 2", true},
               {"key 3", 312},
               {"key 4", nullptr},
               {"key 5", json::object()},
               {"key 6", json::array()}})),
      "key 1: \"string\"\n"
      "key 2: true\n"
      "key 3: 312\n"
      "key 4: null\n"
      "key 5: {}\n"
      "key 6: []\n");
}

TEST(structuredOutputYaml, ArrayOfObjects) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{{"name", "Alice"}, {"age", 25}, {"is_student", true}},
               {{"name", "Bob"}, {"age", 30}, {"is_student", false}}})),
      "- name: \"Alice\"\n"
      "  age: 25\n"
      "  is_student: true\n"
      "- name: \"Bob\"\n"
      "  age: 30\n"
      "  is_student: false\n");
}

TEST(structuredOutputYaml, ArrayOfObjectsFourSpaces) {
  // The keys of an object in an array line up with the first key, which follows the "- ".
  // At the top level that gives the same layout as with two spaces.
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{{"name", "Alice"}, {"age", 25}, {"is_student", true}},
               {{"name", "Bob"}, {"age", 30}, {"is_student", false}}}),
          {.indentation = pep::structuredOutput::WhitespaceFormat::FourSpaces}),
      "- name: \"Alice\"\n"
      "  age: 25\n"
      "  is_student: true\n"
      "- name: \"Bob\"\n"
      "  age: 30\n"
      "  is_student: false\n");
}

TEST(structuredOutputYaml, ObjectOfArrays) {
  const auto tree = Tree::FromJson(
      {{"fruits", {"apple", "banana", "cherry"}}, {"numbers", {33, 22, 11}}, {"flags", {true, false, true}}});

  EXPECT_EQ(
      yaml::to_string(tree, {.includeArraySizeComments = true, .arrayCountCommentThreshold = 3}),
      "fruits: # item count: 3\n"
      "  - \"apple\"\n"
      "  - \"banana\"\n"
      "  - \"cherry\"\n"
      "numbers: # item count: 3\n"
      "  - 33\n"
      "  - 22\n"
      "  - 11\n"
      "flags: # item count: 3\n"
      "  - true\n"
      "  - false\n"
      "  - true\n");

  EXPECT_EQ(
      yaml::to_string(tree),
      "fruits:\n"
      "  - \"apple\"\n"
      "  - \"banana\"\n"
      "  - \"cherry\"\n"
      "numbers:\n"
      "  - 33\n"
      "  - 22\n"
      "  - 11\n"
      "flags:\n"
      "  - true\n"
      "  - false\n"
      "  - true\n");
}

TEST(structuredOutputYaml, NestedArrays) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson({{"matrix4x3", {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {4, 4, 4}}}})),
      "matrix4x3:\n"
      "  - \n"
      "    - 1\n"
      "    - 0\n"
      "    - 0\n"
      "  - \n"
      "    - 0\n"
      "    - 1\n"
      "    - 0\n"
      "  - \n"
      "    - 0\n"
      "    - 0\n"
      "    - 1\n"
      "  - \n"
      "    - 4\n"
      "    - 4\n"
      "    - 4\n");
}

TEST(structuredOutputYaml, NestedArraysSizeComments) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson({{"matrix4x3", {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {4, 4, 4}}}}),
          {.includeArraySizeComments = true, .arrayCountCommentThreshold = 3}),
      "matrix4x3: # item count: 4\n"
      "  - # item count: 3\n"
      "    - 1\n"
      "    - 0\n"
      "    - 0\n"
      "  - # item count: 3\n"
      "    - 0\n"
      "    - 1\n"
      "    - 0\n"
      "  - # item count: 3\n"
      "    - 0\n"
      "    - 0\n"
      "    - 1\n"
      "  - # item count: 3\n"
      "    - 4\n"
      "    - 4\n"
      "    - 4\n");
}

TEST(structuredOutputYaml, MixedTree) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{"list", {1, {2, 3, 2}, 2, 5}},
               {"number", 141},
               {"object", {{"left", {false, true}}, {"right", {{"first", json::array()}, {"second", json::object()}}}}}}),
          {.includeArraySizeComments = true, .arrayCountCommentThreshold = 2}),
      "list: # item count: 4\n"
      "  - 1\n"
      "  - # item count: 3\n"
      "    - 2\n"
      "    - 3\n"
      "    - 2\n"
      "  - 2\n"
      "  - 5\n"
      "number: 141\n"
      "object:\n"
     "  left: # item count: 2\n"
      "    - false\n"
      "    - true\n"
      "  right:\n"
      "    first: []\n"
      "    second: {}\n");
}

TEST(structuredOutputYaml, Indentation) {
  const auto tree = Tree::FromJson(
      {{"outer", {{"inner", {{"deep", "value"}, {"list", {1, 2, 3}}}}}}});

  // Two spaces (default)
  EXPECT_EQ(
      yaml::to_string(tree, {.indentation = pep::structuredOutput::WhitespaceFormat::TwoSpaces}),
      "outer:\n"
      "  inner:\n"
      "    deep: \"value\"\n"
      "    list:\n"
      "      - 1\n"
      "      - 2\n"
      "      - 3\n");

  // Four spaces
  EXPECT_EQ(
      yaml::to_string(tree, {.indentation = pep::structuredOutput::WhitespaceFormat::FourSpaces}),
      "outer:\n"
      "    inner:\n"
      "        deep: \"value\"\n"
      "        list:\n"
      "            - 1\n"
      "            - 2\n"
      "            - 3\n");
}

TEST(structuredOutputYaml, IndentationOfArraysOfObjects) {
  // An array is indented one level below its key and each element starts with "- ".
  // The keys of an object element line up with its first key, right after the "- ";
  // anything nested deeper is indented one more level below the key it belongs to.
  const auto tree = Tree::FromJson(
      {{"people",
        {{{"name", "Alice"}, {"address", {{"city", "Utrecht"}}}, {"pets", {"cat"}}},
         {{"name", "Bob"}}}}});

  // Two spaces (default)
  EXPECT_EQ(
      yaml::to_string(tree, {.indentation = pep::structuredOutput::WhitespaceFormat::TwoSpaces}),
      "people:\n"
      "  - name: \"Alice\"\n"
      "    address:\n"
      "      city: \"Utrecht\"\n"
      "    pets:\n"
      "      - \"cat\"\n"
      "  - name: \"Bob\"\n");

  // Four spaces
  EXPECT_EQ(
      yaml::to_string(tree, {.indentation = pep::structuredOutput::WhitespaceFormat::FourSpaces}),
      "people:\n"
      "    - name: \"Alice\"\n"
      "      address:\n"
      "          city: \"Utrecht\"\n"
      "      pets:\n"
      "          - \"cat\"\n"
      "    - name: \"Bob\"\n");
}

TEST(structuredOutputYaml, EmptyArrayComments) {
  const auto tree = Tree::FromJson(
      {{"empty_array", json::array()},
       {"non_empty", {1, 2}},
       {"nested", {{"also_empty", json::array()}, {"has_data", {3}}}},
       {"matrix", {{1, 2}, json::array(), {3, 4}}}});

  // With empty array comments enabled
  EXPECT_EQ(
      yaml::to_string(tree, {.includeArraySizeComments = true, .arrayCountCommentThreshold = 2, .includeEmptyArrayComments = true}),
      "empty_array: [] # item count: 0\n"
      "non_empty: # item count: 2\n"
      "  - 1\n"
      "  - 2\n"
      "nested:\n"
      "  also_empty: [] # item count: 0\n"
      "  has_data:\n"
      "    - 3\n"
      "matrix: # item count: 3\n"
      "  - # item count: 2\n"
      "    - 1\n"
      "    - 2\n"
      "  - [] # item count: 0\n"
      "  - # item count: 2\n"
      "    - 3\n"
      "    - 4\n");

  // With empty array comments disabled
  EXPECT_EQ(
      yaml::to_string(tree, {.includeArraySizeComments = true, .arrayCountCommentThreshold = 2}),
      "empty_array: []\n"
      "non_empty: # item count: 2\n"
      "  - 1\n"
      "  - 2\n"
      "nested:\n"
      "  also_empty: []\n"
      "  has_data:\n"
      "    - 3\n"
      "matrix: # item count: 3\n"
      "  - # item count: 2\n"
      "    - 1\n"
      "    - 2\n"
      "  - []\n"
      "  - # item count: 2\n"
      "    - 3\n"
      "    - 4\n");
}

TEST(structuredOutputYaml, FromPopulatedTableWithHeader) {
  const auto table = Table::FromSeparateHeaderAndData({"fruit", "color"}, {"apple", "red", "pear", "green"});

  std::ostringstream stream;
  yaml::append(stream, table) << "---\n"; // chaining needs the same stream back

  EXPECT_EQ(
      std::move(stream).str(),
      "metadata:\n"
      "  header:\n"
      "    - \"fruit\"\n"
      "    - \"color\"\n"
      "data:\n"
      "  - fruit: \"apple\"\n"
      "    color: \"red\"\n"
      "  - fruit: \"pear\"\n"
      "    color: \"green\"\n"
      "---\n");
}

TEST(structuredOutputYaml, FromPopulatedTableWithHeaderFourSpaces) {
  const auto table = Table::FromSeparateHeaderAndData({"fruit", "color"}, {"apple", "red", "pear", "green"});

  EXPECT_EQ(
      yaml::to_string(table, {.indentation = pep::structuredOutput::WhitespaceFormat::FourSpaces}),
      "metadata:\n"
      "    header:\n"
      "        - \"fruit\"\n"
      "        - \"color\"\n"
      "data:\n"
      "    - fruit: \"apple\"\n"
      "      color: \"red\"\n"
      "    - fruit: \"pear\"\n"
      "      color: \"green\"\n");
}

TEST(structuredOutputYaml, IntegersBeyondIntRange) {
  EXPECT_EQ(yaml::to_string(Tree::FromJson(5'000'000'000)), "5000000000\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(-5'000'000'000)), "-5000000000\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(std::numeric_limits<std::uint64_t>::max())), "18446744073709551615\n");
}

TEST(structuredOutputYaml, FloatsUseShortestRepresentation) {
  EXPECT_EQ(yaml::to_string(Tree::FromJson(1.5)), "1.5\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(2.0)), "2.0\n") << "should remain recognizable as a float";
  EXPECT_EQ(yaml::to_string(Tree::FromJson(1e-9)), "1e-09\n") << "small values should not be rounded to zero";
}

} // namespace
