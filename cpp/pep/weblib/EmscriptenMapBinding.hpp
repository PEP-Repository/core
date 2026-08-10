#pragma once

#include <pep/utils/CollectionUtils.hpp>

#include <emscripten/bind.h>

namespace emscripten::internal {

// See notes on internal API in EmscriptenVectorBinding.hpp
template <pep::AnyMap Map>
struct BindingType<Map> {
private:
  using ValBinding = BindingType<val>;

public:
  using WireType = ValBinding::WireType;

  /// Serialize
  static WireType toWireType(const Map &map, rvp::default_tag t) {
    val jsMap = val::global("Map").new_();
    for (const auto& [key, value] : map) {
      jsMap.call<void>("set", key, value);
    }
    return ValBinding::toWireType(std::move(jsMap), t);
  }
  /// Serialize
  static WireType toWireType(Map &&map, rvp::default_tag t) {
    val jsMap = val::global("Map").new_();
    for (auto& [key, value] : map) {
      //TODO(workaround) This does not actually move elements yet, see https://github.com/emscripten-core/emscripten/issues/25412
      jsMap.call<void>("set", key, std::move(value));
    }
    return ValBinding::toWireType(std::move(jsMap), t);
  }

  /// Deserialize
  static Map fromWireType(WireType value) {
    // Iterating a JS Map yields its [key, value] entries.
    // Note that we cannot use range adaptors here: `val` provides begin()/end(), but does not model
    // std::ranges::range, because its iterator lacks a difference_type and its operator++ returns void.
    Map map;
    for (const val& entry : ValBinding::fromWireType(value)) {
      map.emplace(
          entry[0].as<typename Map::key_type>(),
          entry[1].as<typename Map::mapped_type>(allow_raw_pointers{}));
    }
    return map;
  }
};

// Type as `any`
template <pep::AnyMap Map>
struct TypeID<Map> : TypeID<val> {};

// Check that our specialization gets selected
static_assert(std::same_as<BindingType<std::unordered_map<int, int>>::WireType, BindingType<val>::WireType>);

}
