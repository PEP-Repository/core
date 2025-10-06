#pragma once

#include <pep/utils/CollectionUtils.hpp>

#include <emscripten/bind.h>

namespace emscripten::internal {

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
      //TODO ideally we would move elements, but see https://github.com/emscripten-core/emscripten/issues/25412
      jsMap.call<void>("set", key, value);
    }
    return ValBinding::toWireType(std::move(jsMap), t);
  }

  /// Deserialize
  static Map fromWireType(WireType value) {
    using namespace std::ranges;
    return pep::RangeToCollection<Map>(
        ValBinding::fromWireType(value)
        | views::transform([](const val& entry) {
          return std::pair{
            entry[0].as<typename Map::key_type>(),
            entry[1].as<typename Map::mapped_type>(allow_raw_pointers{})};
        })
    );
  }
};

// Type as `any`
template <pep::AnyMap Map>
struct TypeID<Map> : TypeID<val> {};

// Check that are specialization gets selected
static_assert(std::same_as<BindingType<std::unordered_map<int, int>>::WireType, BindingType<val>::WireType>);

}
