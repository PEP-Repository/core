#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Tree.hpp>

#include <boost/property_tree/ptree.hpp>

namespace {
using boost::property_tree::ptree;
using nlohmann::json;
using pep::structuredOutput::Tree;
} // namespace

TEST(structuredOutputTreeFromPropertyTree, converts_leaf_to_string) {
  ptree pt;
  pt.put_value("hello");

  const auto tree = Tree::FromPropertyTree(pt);

  EXPECT_EQ(tree.toJson(), json("hello"));
}

TEST(structuredOutputTreeFromPropertyTree, converts_object) {
  ptree pt;
  pt.put("name", "alice");
  pt.put("age", "30");

  const auto tree = Tree::FromPropertyTree(pt);

  EXPECT_EQ(tree.toJson(), json({{"name", "alice"}, {"age", "30"}}));
}

TEST(structuredOutputTreeFromPropertyTree, converts_array) {
  ptree pt;
  ptree child1, child2;
  child1.put_value("one");
  child2.put_value("two");
  pt.push_back({"", child1});
  pt.push_back({"", child2});

  const auto tree = Tree::FromPropertyTree(pt);

  EXPECT_EQ(tree.toJson(), json::array({"one", "two"}));
}

TEST(structuredOutputTreeFromPropertyTree, converts_nested_object) {
  ptree pt;
  ptree child;
  child.put("x", "1");
  child.put("y", "2");
  pt.add_child("point", child);

  const auto tree = Tree::FromPropertyTree(pt);

  EXPECT_EQ(tree.toJson(), json({{"point", {{"x", "1"}, {"y", "2"}}}}));
}

TEST(structuredOutputTreeFromPropertyTree, converts_array_of_objects) {
  ptree pt;
  ptree item1, item2;
  item1.put("key", "a");
  item2.put("key", "b");
  pt.push_back({"", item1});
  pt.push_back({"", item2});

  const auto tree = Tree::FromPropertyTree(pt);

  EXPECT_EQ(tree.toJson(), json::array({{{"key", "a"}}, {{"key", "b"}}}));
}
