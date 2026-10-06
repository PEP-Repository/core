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

} // namespace
