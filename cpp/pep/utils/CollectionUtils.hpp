#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <vector>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace pep {

/*!
 * \brief Returns whether a is a subset of b.
 */
template<typename T>
bool IsSubset(std::vector<T> a, std::vector<T> b) {
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return std::includes(b.begin(), b.end(), a.begin(), a.end());
}

/*!
 * \brief Returns a value that's included multiple times in the vector, or std::nullopt if it contains unique values. Uniqueness is determined by the specified Compare object.
 */
template <typename T, typename TCompare>
std::optional<T> TryFindDuplicateValue(std::vector<T> vec, const TCompare& comp) {
  std::sort(vec.begin(), vec.end(), comp);
  auto position = std::adjacent_find(vec.cbegin(), vec.cend());
  if (position != vec.cend()) {
    return *position;
  }
  return std::nullopt;
}

/*!
 * \brief Returns a value that's included multiple times in the vector, or std::nullopt if it contains unique values. Uniqueness is determined by (a default-constructed instance of) the specified Compare type.
 */
template <typename T, typename TCompare = std::less<T>>
std::optional<T> TryFindDuplicateValue(std::vector<T> vec) {
  return TryFindDuplicateValue(vec, TCompare());
}

/*!
 * \brief Returns whether a vector contains unique values, with uniqueness determined by the specified Compare object.
 */
template <typename T, typename TCompare>
bool ContainsUniqueValues(std::vector<T> vec, const TCompare& comp) {
  return TryFindDuplicateValue(vec, comp) == std::nullopt;
}

/*!
 * \brief Returns whether a vector contains unique values, with uniqueness determined by (a default-constructed instance of) the specified Compare type.
 */
template <typename T, typename TCompare = std::less<T>>
bool ContainsUniqueValues(const std::vector<T>& vec) {
  return TryFindDuplicateValue(vec) == std::nullopt;
}

/*!
 * \brief Given a source vector and a capacity, fill a destination vector with the items of the source until the capacity is reached. An offset can be set to start filling from that index in the source.
 * The size is calculated by iteratively adding the lengths of all items within the source vector with a optional padding added for each of those items. When this number is about to exceed the capacity, filling the destination vector will stop.
 * The resulting size of the destination vector is returned.

 * \param dest destination vector
 * \param source source vector
 * \param cap The max capacity of the destination vector in bytes
 * \param offset The source index from which to start filling.
 * \param padding The amount added to the length of each item.

 * \return The size of the destination vector in bytes.
*/
size_t FillVectorToCapacity(std::vector<std::string>& dest, const std::vector<std::string>& source, const size_t& cap, const size_t& offset = 0, const size_t& padding = 0);

/*
* \brief Determines if a character sequence ends with starting character(s) of another sequence.
* \param haystack The content that may end with (starting characters of) the sought-after sequence.
* \param needle The character sequence to find at the end of the haystack.
* \return The number of starting characters from the needle that occur at the end of the haystack.
*/
size_t FindLongestPrefixAtEnd(std::string_view haystack, std::string_view needle);

template<typename R>
concept Slice = std::ranges::contiguous_range<R> && std::ranges::sized_range<R>;

//XXX This may be removed when we move to C++23, where one can construct a string_view with a range
[[nodiscard]] std::string_view SpanToString(const Slice auto& span)
requires(ByteLike<std::ranges::range_value_t<decltype(span)>>) {
  return {reinterpret_cast<const char*>(std::ranges::data(span)), std::ranges::size(span)};
}

/// \throws std::invalid_argument if \p span does not have \p Extent elements
template<size_t Extent>
[[nodiscard]] auto ToSizedSpan(const Slice auto& span) {
  if constexpr (Extent != std::dynamic_extent) {
    if (std::ranges::size(span) != Extent) {
      throw std::invalid_argument("Argument has incorrect number of elements");
    }
  }
  // range_value_t does not retain const
  using Elem = std::remove_reference_t<std::ranges::range_reference_t<decltype(span)>>;
  return std::span<Elem, Extent>{reinterpret_cast<Elem*>(std::ranges::data(span)), std::ranges::size(span)};
}

template<ByteLike To>
[[nodiscard]] auto ConvertBytes(const Slice auto& span)
  requires(ByteLike<std::ranges::range_value_t<decltype(span)>>) {
  // range_value_t does not retain const
  using From = std::remove_reference_t<std::ranges::range_reference_t<decltype(span)>>;
  return std::span<CopyConstness<To, From>>{reinterpret_cast<CopyConstness<To, From>*>(std::ranges::data(span)), std::ranges::size(span)};
}

template<ByteLike To, ByteLike From = std::byte, size_t Extent = std::dynamic_extent>
[[nodiscard]] auto ConvertBytes(std::span<From, Extent> span) {
  return std::span<CopyConstness<To, From>, Extent>{reinterpret_cast<CopyConstness<To, From>*>(span.data()), span.size()};
}

template<typename Elem, size_t Extent> requires(Extent != std::dynamic_extent)
[[nodiscard]] std::array<Elem, Extent> SpanToArray(std::span<const Elem, Extent> span) {
  //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) Size was statically checked
  std::array<Elem, Extent> array;
  std::ranges::copy(span, array.begin());
  return array;
}

namespace detail {
  template <typename C>
  concept CanReserve = requires(C c, std::size_t size) {
      c.reserve({size});
  };
}

//XXX This should be removed in C++23 with std::ranges::to, std::from_range, assign/insert_range
template <typename ResultCollection, std::ranges::input_range Range>
//NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) We just want to bind to anything
[[nodiscard]] auto RangeToCollection(Range&& range) {
  using namespace std::ranges;
  if constexpr (std::constructible_from<ResultCollection, iterator_t<Range>, sentinel_t<Range>>) {
    return ResultCollection(begin(range), end(range));
  } else { // Construction with begin/end may not work if iterator_t<Range> != sentinel_t<Range>
    ResultCollection result;
    if constexpr (sized_range<Range> && detail::CanReserve<ResultCollection>) {
      result.reserve(size(range));
    }
    copy(range, std::inserter(result, end(result)));
    return result;
  }
}
template <template <typename...> class ResultCollection>
[[nodiscard]] auto RangeToCollection(std::ranges::input_range auto&& range) {
  return RangeToCollection<ResultCollection<std::ranges::range_value_t<decltype(range)>>>(range);
}
[[nodiscard]] auto RangeToVector(std::ranges::input_range auto&& range) {
  return RangeToCollection<std::vector>(range);
}

//XXX This should be removed in C++23 with std::views::as_rvalue
/// Range adapter to make all elements in a range rvalue references.
/// \details Example usage:
/// \code
///   CollectVec(range | MoveElements)
/// \endcode
constexpr auto MoveElements = std::views::transform([](auto& elem) { return std::move(elem); });

/// Returns optional with the single element in the range, if any.
/// \throws std::out_of_range if the range contains multiple elements.
[[nodiscard]] auto RangeToOptional(std::ranges::input_range auto&& range)
  -> std::optional<std::ranges::range_value_t<decltype(range)>> {
  using namespace std::ranges;
  auto it = begin(range);
  const auto endIt = end(range);
  if (it == endIt) { return {}; }
  auto result = *it;
  if (++it != endIt) {
    throw std::out_of_range("range contains multiple elements");
  }
  return result;
}

auto OnlyItemIn(auto&& container) {
  const auto begin = container.begin(), end = container.end();
  if (begin == end) {
    throw std::runtime_error("Can't get item from empty container");
  }
  if (auto i = begin; ++i != end) {
    throw std::runtime_error("Container has more than one item");
  }
  return *begin;
}

}
