#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <vector>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

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
 * \brief Fills a destination range with strings from a source range without exceeding the specified destination capacity.

 * \tparam TDest the type of destination iterator
 * \tparam TSrc the type of source range

 * \param dest destination iterator
 * \param cap The max capacity of the destination in bytes
 * \param src the source range
 * \param padding bytes of overhead associated with each string. Specify a non-zero value to take destination storage overhead into account; add one to take the strings' NULterminators into account.

 * \return The number of bytes written to the destination.
*/
template <std::output_iterator<std::string> TDest, std::ranges::input_range TSrc>
  requires std::same_as<std::remove_cvref_t<std::ranges::range_value_t<TSrc>>, std::string>
size_t FillToCapacity(TDest dest, size_t cap, const TSrc& src, size_t padding = 0) {
  size_t bytesWritten{ 0 };
  for (const auto& item: src) {
    auto add = item.length() + padding;
    if (bytesWritten + add > cap) {
      break;
    }
    *dest++ = item;
    bytesWritten += add;
  }
  return bytesWritten;
}

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
[[nodiscard]] auto ConvertBytes(Slice auto&& span)
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

/// Copy elements from src to dst, stopping when either range reaches the end
constexpr auto CopyToRange(
  std::ranges::input_range auto&& src,
  std::ranges::output_range<std::ranges::range_value_t<decltype(src)>> auto&& dst)
requires (std::ranges::sized_range<decltype(src)> && std::ranges::sized_range<decltype(dst)>) {
  using namespace std::ranges;
  using IterDiff = range_difference_t<decltype(src)>;
  const IterDiff copySize{std::min(
      static_cast<IterDiff>(size(src)),
      static_cast<IterDiff>(size(dst)))};
  return copy_n(
    begin(std::forward<decltype(src)>(src)),
    copySize,
    begin(std::forward<decltype(dst)>(dst)));
}

/// Copy src to dst, checking that it fits
/// \param exact If \p dst should be required to be the exact same size as \p src
constexpr auto CheckedCopy(
  std::ranges::input_range auto&& src,
  std::ranges::output_range<std::ranges::range_value_t<decltype(src)>> auto& dst,
  bool exact = false)
requires (std::ranges::sized_range<decltype(src)> && std::ranges::sized_range<decltype(dst)>) {
  using namespace std::ranges;
  if (exact) {
    if (std::cmp_not_equal(size(dst), size(src))) {
      throw std::out_of_range("src range is not the same size as dst");
    }
  } else {
    if (std::cmp_less(size(dst), size(src))) {
      throw std::out_of_range("dst range is smaller than src");
    }
  }
  return copy(std::forward<decltype(src)>(src), begin(dst));
}

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

template <typename T>
concept AnyMap = DerivedFromSpecialization<T, std::map> || DerivedFromSpecialization<T, std::unordered_map>;

/// @brief Adds items from a range to an \ref std::set, throwing an exception if an item could not be inserted because it's a duplicate
/// @tparam T the type of item in the \ref std::set
/// @tparam TSrc the type of the input range
/// @param dst the destination \ref std::set
/// @param src the source range
/// @return a pair of (1) an iterator at the last insertion position and (2) the number of items inserted into the set
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
