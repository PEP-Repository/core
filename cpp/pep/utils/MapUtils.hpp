#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <pep/utils/TypeTraits.hpp>

namespace pep {

template <typename T>
class DereferenceHash {
  std::hash<std::remove_cv_t<T>> inner_;
public:
  [[nodiscard]] std::size_t operator()(const T* ptr) const noexcept(noexcept(inner_(*ptr))) {
    return inner_(*ptr);
  }
};

template <typename T>
class DereferenceEquals {
public:
  [[nodiscard]] bool operator()(const T* a, const T* b) const noexcept(noexcept(*a == *b)) {
    return *a == *b;
  }
};

template <typename T>
using UnorderedPointerSet = std::unordered_set<T*, DereferenceHash<T>, DereferenceEquals<T>>;

/// Returns whether \p sub is a subset of \p super .
///
/// Ignores duplicate values.
bool IsSubset(std::ranges::input_range auto&& sub, std::ranges::forward_range auto&& super) {
  using namespace std::ranges;
  UnorderedPointerSet<const range_value_t<decltype(super)>> superset;
  if constexpr (sized_range<decltype(super)>) {
    superset.reserve(super.size());
  }
  // O(super*log(super))
  for (const auto& value : super) { superset.insert(&value); }
  // O(sub*log(super))
  return all_of(sub, [&](const auto& value) { return superset.contains(&value); });
}

/*!
 * \brief Returns a value that's included multiple times in the vector, or std::nullopt if it contains unique values.
 */
auto TryFindDuplicateValue(std::ranges::forward_range auto&& values)
-> std::optional<std::ranges::range_value_t<decltype(values)>> {
  using namespace std::ranges;
  using T = range_value_t<decltype(values)>;
  UnorderedPointerSet<const T> set;
  if constexpr (sized_range<decltype(values)>) {
    set.reserve(size(values));
  }
  for (const T& value : values) {
    if (!set.insert(std::addressof(value)).second) {
      return std::optional<T>{value};
    }
  }
  return std::nullopt;
}

/*!
 * \brief Returns whether a vector contains unique values.
 */
bool ContainsUniqueValues(std::ranges::forward_range auto&& values) {
  return !TryFindDuplicateValue(values);
}

template <typename T>
concept AnyMap = DerivedFromSpecialization<T, std::map> || DerivedFromSpecialization<T, std::unordered_map>;

}
