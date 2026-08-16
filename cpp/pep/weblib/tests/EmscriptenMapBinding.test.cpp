#include <pep/weblib/EmscriptenMapBinding.hpp>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace emscripten;
using namespace pep::weblib::tests;

namespace {

using StringMap = std::map<std::string, std::string>;
using UnorderedStringMap = std::unordered_map<std::string, std::string>;

/// Renders a cpp map as a string. Renders through an std::map, so that the order is deterministic.
template <pep::AnyMap Map>
std::string DescribeMapCpp(const Map& map) {
  std::string result;
  for (const auto& [key, value] : StringMap(map.begin(), map.end())) {
    result += key + '=' + value + ';';
  }
  return result;
}

/// Renders a JS Map by calling the JS Map API
std::string DescribeMapJs(const val& value) {
  if (!value.instanceof(val::global("Map"))) {
    return "not a Map: " + val::global("String")(value).as<std::string>();
  }
  // Sort the keys, so that the rendered order is deterministic.
  val keys = val::global("Array").call<val>("from", value.call<val>("keys"));
  keys.call<val>("sort");

  std::string result;
  for (auto i = 0U; i < keys["length"].as<unsigned>(); ++i) {
    const val key = keys[i];
    result += key.as<std::string>() + '=' + value.call<val>("get", key).as<std::string>() + ';';
  }
  return result;
}

/// Bound: takes and returns a map, so that it is both deserialized and serialized by our binding.
template <pep::AnyMap Map>
Map EchoMap(Map map) { return map; }

EMSCRIPTEN_BINDINGS(EmscriptenMapBindingTest) {
  function("pepTestEchoMap", &EchoMap<StringMap>);
  function("pepTestEchoUnorderedMap", &EchoMap<UnorderedStringMap>);
}

/// Creates a JS Map containing \p entries.
val MakeJsMap(const std::vector<std::pair<std::string, std::string>>& entries) {
  val jsMap = val::global("Map").new_();
  for (const auto& [key, value] : entries) {
    jsMap.call<void>("set", key, value);
  }
  return jsMap;
}

/// Pass a JS Map containing \p entries to C++, and return how it arrived there.
/// Runs on the main thread, where our binding and the Embind type registry live.
template <pep::AnyMap Map>
std::string PassJsMapToCpp(std::vector<std::pair<std::string, std::string>> entries) {
  return PromiseTest([entries = std::move(entries)] {
    return val::global("Promise").call<val>("resolve", DescribeMapCpp(MakeJsMap(entries).as<Map>()));
  });
}

/// Pass \p map to JS as an rvalue, which selects the rvalue overload of toWireType.
template <pep::AnyMap Map>
std::string PassCppMapToJs(Map map) {
  return PromiseTest([map = std::move(map)]() mutable {
    return val::global("Promise").call<val>("resolve", DescribeMapJs(val(std::move(map))));
  });
}

/// Pass \p map to JS as an lvalue, which selects the const reference overload of toWireType.
template <pep::AnyMap Map>
std::string PassCppMapCopyToJs(const Map& map) {
  return PromiseTest([&map] {
    return val::global("Promise").call<val>("resolve", DescribeMapJs(val(map)));
  });
}

TEST(EmscriptenMapBinding, fromWireType) {
  EXPECT_EQ(PassJsMapToCpp<StringMap>({{"b", "2"}, {"a", "1"}}), "a=1;b=2;") << "Should receive every entry of the JS Map";
}

TEST(EmscriptenMapBinding, fromWireTypeEmpty) {
  EXPECT_EQ(PassJsMapToCpp<StringMap>({}), "") << "An empty JS Map should produce an empty C++ map";
}

TEST(EmscriptenMapBinding, fromWireTypeUnorderedMap) {
  EXPECT_EQ(PassJsMapToCpp<UnorderedStringMap>({{"b", "2"}, {"a", "1"}}), "a=1;b=2;");
}

TEST(EmscriptenMapBinding, duplicateKeys) {
  EXPECT_EQ(PassJsMapToCpp<StringMap>({{"a", "1"}, {"b", "2"}, {"a", "3"}}),"a=3;b=2;");
}

TEST(EmscriptenMapBinding, toWireType) {
  EXPECT_EQ(PassCppMapToJs(StringMap{{"a", "1"}, {"b", "2"}}), "a=1;b=2;") << "JS should receive a Map with every entry";
}

TEST(EmscriptenMapBinding, toWireTypeEmpty) {
  EXPECT_EQ(PassCppMapToJs(StringMap{}), "") << "An empty C++ map should produce an empty JS Map";
}

TEST(EmscriptenMapBinding, toWireTypeUnorderedMap) {
  EXPECT_EQ(PassCppMapToJs(UnorderedStringMap{{"a", "1"}, {"b", "2"}}), "a=1;b=2;");
}

TEST(EmscriptenMapBinding, toWireTypeCopy) {
  const StringMap map{{"a", "1"}, {"b", "2"}};
  EXPECT_EQ(PassCppMapCopyToJs(map), "a=1;b=2;");
  EXPECT_EQ(DescribeMapCpp(map), "a=1;b=2;") << "Should leave the copied map intact";
}

TEST(EmscriptenMapBinding, boundFunctionSignature) {
  // Chained on the main thread, because a val cannot be accessed from another thread.
  const std::string described = PromiseTest([] {
    const val echoed = val::module_property("pepTestEchoMap")(MakeJsMap({{"b", "2"}, {"a", "1"}}));
    return val::global("Promise").call<val>("resolve", DescribeMapJs(echoed));
  });
  EXPECT_EQ(described, "a=1;b=2;") << "A bound function should be able to take and return a map";
}

TEST(EmscriptenMapBinding, boundFunctionSignatureUnorderedMap) {
  const std::string described = PromiseTest([] {
    const val echoed = val::module_property("pepTestEchoUnorderedMap")(MakeJsMap({{"b", "2"}, {"a", "1"}}));
    return val::global("Promise").call<val>("resolve", DescribeMapJs(echoed));
  });
  EXPECT_EQ(described, "a=1;b=2;");
}

}
