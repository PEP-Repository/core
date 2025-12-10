#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Yaml.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
namespace yaml = pep::structuredOutput::yaml;
using pep::structuredOutput::Tree;


TEST(structuredOutputYaml, NakedValues) {
  // naked values
  EXPECT_EQ(yaml::to_string(Tree::FromJson("this string")), "\"this string\"\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(false)), "false\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(52)), "52\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(nullptr)), "null\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(nlohmann::json::object())), "{}\n");
  EXPECT_EQ(yaml::to_string(Tree::FromJson(nlohmann::json::array())), "[]\n");
}

TEST(structuredOutputYaml, FlatArray) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson({"simple string", true, 17, nullptr, nlohmann::json::object(), nlohmann::json::array()})),
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
      "A: null\n"
      "B: null\n"
      "C: null\n"
      "D: null\n")
      << "keys should appear in alphabetical order";

  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{"key 1", "string"},
               {"key 2", true},
               {"key 3", 312},
               {"key 4", nullptr},
               {"key 5", nlohmann::json::object()},
               {"key 6", nlohmann::json::array()}})),
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
      "- age: 25\n"
      "  is_student: true\n"
      "  name: \"Alice\"\n"
      "- age: 30\n"
      "  is_student: false\n"
      "  name: \"Bob\"\n");
}

TEST(structuredOutputYaml, ObjectOfArrays) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{"fruits", {"apple", "banana", "cherry"}}, {"numbers", {33, 22, 11}}, {"flags", {true, false, true}}})),
      "flags: # size = 3\n"
      "  - true\n"
      "  - false\n"
      "  - true\n"
      "fruits: # size = 3\n"
      "  - \"apple\"\n"
      "  - \"banana\"\n"
      "  - \"cherry\"\n"
      "numbers: # size = 3\n"
      "  - 33\n"
      "  - 22\n"
      "  - 11\n");
}

TEST(structuredOutputYaml, MixedTree) {
  EXPECT_EQ(
      yaml::to_string(
          Tree::FromJson(
              {{"list", {1, {2, 3, 2}, 2, 5}},
               {"number", 141},
               {"object",
                {{"left", {false, true}},
                 {"right", {{"first", nlohmann::json::array()}, {"second", nlohmann::json::object()}}}}}})),
      "list: # size = 4\n"
      "  - 1\n"
      "  - # size = 3\n"
      "    - 2\n"
      "    - 3\n"
      "    - 2\n"
      "  - 2\n"
      "  - 5\n"
      "number: 141\n"
      "object:\n"
      "  left: # size = 2\n"
      "    - false\n"
      "    - true\n"
      "  right:\n"
      "    first: []\n"
      "    second: {}\n");
}

} // namespace
