#pragma once

#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace pep {

/*!
 * \brief Returns whether a is a subset of b.
 */
template<typename T>
bool isSubset(std::vector<T> a, std::vector<T> b) {
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

/// Usage:
/// \code
///   const std::set<std::string>& get() { return Default<std::set<std::string>>; }
/// \endcode
///
/// Or e.g.:
/// \code
///   funcTakingRef(cond ? myVector : Default<decltype(myVector)>)
/// \endcode
template <typename T>
inline const T Default;

/*
* \brief Converts a weak_ptr<> to one type to a weak_ptr<> to another type.
*/
template <typename TDest, typename TSource>
std::weak_ptr<TDest> static_pointer_cast(std::weak_ptr<TSource> p) {
  // https://stackoverflow.com/a/26534120
  return std::static_pointer_cast<TDest>(p.lock());
}

/*
* \brief Converts a boolean to a string representation.
* \param value The boolean to convert.
* \return The specified boolean's string representation.
*/
std::string BoolToString(bool value);

/*
* \brief Converts a string representation to a boolean.
* \param value The string to convert.
* \return The boolean written out in the specified string.
*/
bool StringToBool(const std::string& value);

/* \brief Gets an optional<Value> from an optional<Owner>.
 * \param owner The (possibly nullopt) value from which to retrieve a value.
 * \param getValue A function that returns a value when invoked with an Owner instance.
 * \return std::nullopt if owner is nullopt; otherwise the result of invoking the the getValue function on the owner.
 */
template <typename T, typename TGetValue>
auto GetOptionalValue(const std::optional<T>& owner, const TGetValue& getValue) -> std::optional<decltype(getValue(*owner))> {
  if (!owner) {
    return std::nullopt;
  }
  return getValue(*owner);
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

template<typename T>
concept ByteLike = sizeof(T) == 1;

template<typename R>
concept Slice = std::ranges::contiguous_range<R> && std::ranges::sized_range<R>;

template<typename T, typename Ref>
using CopyConstness = std::conditional_t<std::is_const_v<Ref>, const T, T>;

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

template<typename T>
[[nodiscard]] std::optional<T> ConvertOptional(boost::optional<T> opt) {
  if (opt) {
    return {std::move(*opt)};
  } else {
    return {};
  }
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
concept Enum = std::is_enum_v<T>;

//XXX Replace by std::to_underlying in C++23
[[nodiscard]] auto ToUnderlying(Enum auto v) {
  return static_cast<std::underlying_type_t<decltype(v)>>(v);
}

/// Strip the first element from a tuple
template <typename Head, typename... Tail>
[[nodiscard]] std::tuple<Tail...> TupleTail(std::tuple<Head, Tail...> tuple) {
  return std::apply([](const auto&, auto&&... tail) {
      return std::tuple{std::move(tail)...};
  }, std::move(tuple));
}

/// Overloaded function to unwrap tuple containing a single element
template <typename... T>
[[nodiscard]] std::tuple<T...> TryUnwrapTuple(std::tuple<T...> tuple) {
  return std::move(tuple);
}
template <typename T>
[[nodiscard]] T TryUnwrapTuple(std::tuple<T> tuple) {
  return std::get<0>(std::move(tuple));
}

//XXX Replace by auto(v) in C++23
[[nodiscard]] auto decay_copy(auto v) { return v; }

// Wrap overloaded/templated function in lambda object to pass to another function
#define PEP_WrapOverloadedFunction(fun) \
  ([](auto&&... args) -> decltype(auto) { \
    return (fun)(std::forward<decltype(args)>(args)...); \
  })

/// Create a ptree path with separator set to '\0' to prevent splitting on '.'
boost::property_tree::path RawPtreePath(const std::string& path);

}
