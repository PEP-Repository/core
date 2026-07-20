#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <set>
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

/// @brief Adds items from a range to an \ref std::set, throwing an exception if an item could not be inserted because it's a duplicate
/// @tparam T the type of item in the \ref std::set
/// @tparam TSrc the type of the input range
/// @param dst the destination \ref std::set
/// @param src the source range
/// @return a pair of (1) an iterator at the last insertion position and (2) the number of items inserted into the set
/// @throws whatever dst throws when an insertion fails, or an \ref std::runtime_error if one of \p src 's items is a duplicate.
/// @remark Provides a basic (as opposed to strong) exception guarantee: if an exception is raised because of a duplicate item, \p dst may have been partially updated.
template <typename T, std::ranges::input_range TSrc>
auto InsertNonDuplicates(std::set<T>& dst, const TSrc& src)
  requires (std::same_as<T, std::remove_cvref_t<std::ranges::range_value_t<TSrc>>>) {
  auto last = dst.end();
  size_t count = 0U;
  for (const auto& item : src) {
    auto sizeBeforeInsertion = dst.size();
    last = dst.insert(last, item);
    if (dst.size() == sizeBeforeInsertion) { // https://cppreference.com/cpp/container/set/insert: "One way to check success of a hinted insert is to compare size() before and after."
      throw std::runtime_error("Can't insert duplicate item into set");
    }
    ++count;
  }
  return std::make_pair(last, count);
}

}
