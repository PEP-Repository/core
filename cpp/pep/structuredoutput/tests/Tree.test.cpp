#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/Tree.hpp>

#include <type_traits>
#include <utility>

namespace {
using json = nlohmann::ordered_json;
using pep::structuredOutput::Tree;

// On a Tree that stays alive (lvalue), rawJson() returns a const reference to the stored json, so it is not copied
static_assert(std::is_same_v<decltype(std::declval<const Tree&>().rawJson()), const json&>);
// On a Tree that is about to go away (rvalue, e.g. std::move(tree).rawJson()), rawJson() returns the json by value,
// moving it out of the Tree instead of copying it
static_assert(std::is_same_v<decltype(std::declval<Tree&&>().rawJson()), json>);

// The static_asserts above only check the return types. This checks that the rvalue overload really moves:
// the extracted json has the original contents, and the Tree is left empty (null) rather than keeping a copy.
TEST(structuredOutputTree, raw_json_on_an_rvalue_hands_over_the_stored_json) {
  auto tree = Tree::FromJson({{"key", "value"}});

  const auto extracted = std::move(tree).rawJson();

  EXPECT_EQ(extracted, json({{"key", "value"}}));
  EXPECT_TRUE(tree.rawJson().is_null()) // NOLINT(bugprone-use-after-move) deliberately inspecting the moved-from state
      << "the json should have been moved out of the tree rather than copied";
}

} // namespace
