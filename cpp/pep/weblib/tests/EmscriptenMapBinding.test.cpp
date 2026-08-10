#include <pep/weblib/EmscriptenMapBinding.hpp>

#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace emscripten;
using namespace pep::weblib::tests;

namespace {

/// Renders the map that C++ received, so that tests can assert on what crossed the boundary.
/// Takes an \c std::map, so that the rendered order is deterministic.
std::string DescribeMap(std::map<std::string, std::string> map) {
  std::string result;
  for (const auto& [key, value] : map) {
    result += key + '=' + value + ';';
  }
  return result;
}

EMSCRIPTEN_BINDINGS(EmscriptenMapBindingTest) {
  function("pepTestDescribeMap", &DescribeMap);
}

/// Pass a JS Map containing \p entries to C++, and return how it arrived there.
/// Runs on the main thread, where our binding and the Embind type registry live.
std::string PassToCpp(std::vector<std::pair<std::string, std::string>> entries) {
  return PromiseTest([entries = std::move(entries)] {
    val jsMap = val::global("Map").new_();
    for (const auto& [key, value] : entries) {
      jsMap.call<void>("set", key, value);
    }
    return val::global("Promise").call<val>("resolve",
        val::module_property("pepTestDescribeMap")(jsMap));
  });
}

TEST(EmscriptenMapBinding, fromWireType) {
  EXPECT_EQ(PassToCpp({{"b", "2"}, {"a", "1"}}), "a=1;b=2;") << "Should receive every entry of the JS Map";
}

TEST(EmscriptenMapBinding, fromWireTypeEmpty) {
  EXPECT_EQ(PassToCpp({}), "") << "An empty JS Map should produce an empty C++ map";
}

}
