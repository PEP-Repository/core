#pragma once

#include <emscripten/bind.h>

#include <vector>

namespace emscripten::internal {

// Embind: (de)serialize vectors as JS arrays, based on https://github.com/emscripten-core/emscripten/issues/11070#issuecomment-717675128.
// The API for this is not officially stable, but not using it means massively complicating (de)serialization.
// Note that emscripten::register_vector would instead return a JS wrapper object.
//
// References:
// - How stable is the API: https://github.com/emscripten-core/emscripten/discussions/15834
// - Stabilize the API: https://github.com/emscripten-core/emscripten/issues/9022
// - (De)serialize standard collections as their JS counterparts: https://github.com/emscripten-core/emscripten/issues/11070
template <typename T, typename Allocator>
struct BindingType<std::vector<T, Allocator>> {
private:
  using Vec = std::vector<T, Allocator>;
  using ValBinding = BindingType<val>;

public:
  using WireType = ValBinding::WireType;

  /// Serialize
  static WireType toWireType(const Vec &vec, rvp::default_tag t) {
    return ValBinding::toWireType(val::array(vec), t);
  }
  /// Serialize
  static WireType toWireType(Vec &&vec, rvp::default_tag t) {
    //XXX This does not actually move elements yet, see https://github.com/emscripten-core/emscripten/issues/25412
    return ValBinding::toWireType(val::array(
      std::move_iterator(vec.begin()),
      std::move_iterator(vec.end())), t);
  }

  /// Deserialize
  static Vec fromWireType(WireType value) {
    return vecFromJSArray<T>(ValBinding::fromWireType(value), allow_raw_pointers{});
  }
};

// Type as `any`
template <typename T, typename Allocator>
struct TypeID<std::vector<T, Allocator>> : TypeID<val> {};

// Check that our specialization gets selected
static_assert(std::same_as<BindingType<std::vector<int>>::WireType, BindingType<val>::WireType>);

}
