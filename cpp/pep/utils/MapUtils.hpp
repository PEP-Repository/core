#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/TypeTraits.hpp>

namespace pep {

/// Hash pointees.
template <typename T>
class DereferenceHash {
  std::hash<std::remove_cv_t<T>> inner_;
public:
  [[nodiscard]] std::size_t operator()(const T* ptr) const noexcept(noexcept(inner_(*ptr))) {
    return inner_(*ptr);
  }
};

/// Compare pointees for equality.
template <typename T>
class DereferenceEquals {
public:
  [[nodiscard]] bool operator()(const T* a, const T* b) const noexcept(noexcept(*a == *b)) {
    return *a == *b;
  }
};

/// unordered_set of pointers that hashes/compares the pointees.
template <typename T>
using UnorderedPointerSet = std::unordered_set<T*, DereferenceHash<T>, DereferenceEquals<T>>;

/// Allocates capacity in \p container to match the size of \p source, if \p source is a sized range.
void ReserveToMatch(std::ranges::sized_range auto& container, std::ranges::forward_range auto&& source) {
  if constexpr (std::ranges::sized_range<decltype(source)>) { container.reserve(source.size()); }
}

/// \brief Builds a pointer set from a range, collecting duplicates separately.
/// \details Inserts pointers to each element into an unordered set; elements that
/// are already present are appended to the duplicates vector instead.
/// \returns A pair with (first) a set of unique element pointers, and
/// (second) a vector of pointers to the duplicate elements.
auto MakeUnorderedPointerSet(std::ranges::forward_range auto&& values)
    -> std::pair<
        UnorderedPointerSet<QualifiedRangeValue<decltype(values)>>,
        std::vector<QualifiedRangeValue<decltype(values)>*>> {
  auto result = decltype(MakeUnorderedPointerSet(values)){};
  auto& [set, duplicates] = result;
  ReserveToMatch(set, values);
  for (auto& value : values) {
    auto ptr = std::addressof(value);
    if (!set.insert(ptr).second) { duplicates.emplace_back(ptr); }
  }
  return result;
}

/// \brief Returns whether \p sub is a subset of \p super .
/// \details Ignores duplicate values.
bool IsSubset(std::ranges::input_range auto const& sub, std::ranges::forward_range auto const& super) {
  using namespace std::ranges;
  const auto superset = MakeUnorderedPointerSet(super).first; // O(super*log(super))
  return all_of(sub, [&](const auto& value) { return superset.contains(&value); }); // O(sub*log(super))
}

/// Returns a value that's included multiple times in the vector, or nullptr if it contains unique values.
auto TryFindDuplicateValue(std::ranges::forward_range auto&& values) -> QualifiedRangeValue<decltype(values)>* {
  using namespace std::ranges;
  const auto duplicates = MakeUnorderedPointerSet(values).second;
  return duplicates.empty() ? nullptr : duplicates.front();
}

/// \brief Returns a value that's included in both vectors, or std::nullopt if no such value exists.
/// \details Equality is determined by the specified Compare object.
auto TryFindCommonValue(std::ranges::forward_range auto&& valuesA, std::ranges::forward_range auto&& valuesB) -> QualifiedRangeValue<decltype(valuesA)>* {
  using namespace std::ranges;
  const auto setA = MakeUnorderedPointerSet(valuesA).first;
  const auto setB = MakeUnorderedPointerSet(valuesB).first;
  const auto commonVal = find_if(setB, [&](auto ptr) { return setA.contains(ptr); });
  return (commonVal != setB.end()) ? *commonVal : nullptr;
}

/// Returns whether a vector contains unique values.
bool ContainsUniqueValues(std::ranges::forward_range auto&& values) {
  return !TryFindDuplicateValue(values);
}

template <typename T>
concept AnyMap = DerivedFromSpecialization<T, std::map> || DerivedFromSpecialization<T, std::unordered_map>;

/// \brief Adds items from a range to an \ref std::set, throwing an exception if an item could not be inserted because it's a duplicate
/// \tparam T the type of item in the \ref std::set
/// \tparam TSrc the type of the input range
/// \param dst the destination \ref std::set
/// \param src the source range
/// \return a pair of (1) an iterator at the last insertion position and (2) the number of items inserted into the set
/// \throws whatever dst throws when an insertion fails, or an \ref std::runtime_error if one of \p src 's items is a duplicate.
/// \remark Provides a basic (as opposed to strong) exception guarantee: if an exception is raised because of a duplicate item, \p dst may have been partially updated.
/// \note Different from \c std::set::insert_range , which silently ignores duplicates.
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
