#pragma once

#include <pep/utils/TypeTraits.hpp>
#include <any>
#include <cassert>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>

namespace pep {

class TaggedValues {
private:
  std::unordered_map<std::type_index, std::any> mValues;

  template <typename TTag> static constexpr std::type_index KeyFor() noexcept { return std::type_index(typeid(TTag)); }
  template <typename TTag, typename TValues> static CopyConstness<typename TTag::type, TValues>* ConstAgnosticGet(TValues& values);

public:
  template <typename TTag> typename TTag::type* get() noexcept { return ConstAgnosticGet<TTag>(mValues); }
  template <typename TTag> const typename TTag::type* get() const noexcept { return ConstAgnosticGet<TTag>(mValues); }

  template <typename TTag> void set(typename TTag::type value) { mValues.insert_or_assign(KeyFor<TTag>(), std::move(value)); }
  template <typename TTag> void unset() { mValues.erase(KeyFor<TTag>()); }

  void clear() { mValues.clear(); }

  size_t empty() const noexcept { return mValues.empty(); }
  size_t size() const noexcept { return mValues.size(); }
};


template <typename TTag, typename TValues>
CopyConstness<typename TTag::type, TValues>* TaggedValues::ConstAgnosticGet(TValues& values) {
  static_assert(std::is_same_v<decltype(TaggedValues::mValues), std::remove_const_t<TValues>>, "Parameter must be (possibly const qualified) TaggedValues::mValues");
  using Value = CopyConstness<typename TTag::type, TValues>;
  Value* result = nullptr;

  auto position = values.find(KeyFor<TTag>());
  if (position != values.end()) {
    auto& untyped = position->second;
    result = std::any_cast<Value>(&untyped);
    assert(result != nullptr);
  }

  return result;
}

}
