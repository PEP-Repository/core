#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace pep {

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
bool StringToBool(std::string_view value);

//XXX This may be removed in favor of optional::transform when we move to C++23
/* \brief Gets an optional<Value> from an optional<Owner>.
 * \param owner The (possibly nullopt) value from which to retrieve a value.
 * \param getValue A function that returns a value when invoked with an Owner instance.
 * \return std::nullopt if owner is nullopt; otherwise the result of invoking the the getValue function on the owner.
 */
template <DerivedFromSpecialization<std::optional> TOptional>
auto GetOptionalValue(TOptional&& owner, auto&& getValue)
    -> std::optional<std::decay_t<decltype(std::forward<decltype(getValue)>(getValue)(*owner))>> {
  if (!owner) {
    return std::nullopt;
  }
  return std::forward<decltype(getValue)>(getValue)(*std::forward<decltype(owner)>(owner));
}

template<typename T>
[[nodiscard]] std::optional<T> ConvertOptional(boost::optional<T> opt) {
  if (opt) {
    return {std::move(*opt)};
  } else {
    return {};
  }
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

/// Create a ptree path with separator set to '\0' to prevent splitting on '.'
boost::property_tree::path RawPtreePath(const std::string& path);

// Wrap overloaded/templated function in lambda object to pass to another function
#define PEP_WrapFn(fun) \
  ([](auto&&... args) -> decltype(auto) { \
    return (fun)(std::forward<decltype(args)>(args)...); \
  })

// U+00B5 (micro symbol) encoded in UTF-8, plus NULterminator
constexpr char micro_symbol[] = { char(0xC2), char(0xB5), char(0x0) };

/// @brief Returns the SI prefix for the specified power
/// @tparam T The (unsigned integral) type of the values that are passed as this function's parameters
/// @param power The power to express as a unit prefix
/// @return The unit prefix for the specified power, or nullopt if no prefix applies to it
/// @remark Only preferred prefixes are returned, i.e. powers that are a multiple of 3 (no "h" or "da").
template <std::integral T>
std::optional<std::string> SiPrefix(T power) {
  if constexpr (std::is_signed_v<T>) {
    switch (power) {
    case T(-1): return "d";
    case T(-2): return "c";
    case T(-3): return "m";
    case T(-6): return micro_symbol;
    case T(-9): return "n";
    case T(-12): return "p";
    case T(-15): return "f";
    case T(-18): return "a";
    case T(-21): return "z";
    case T(-24): return "y";
    case T(-27): return "r";
    case T(-30): return "q";
    default: break; // Try to match positive value (below)
    }
  }

  switch (power) {
  case T(1): return "da";
  case T(2): return "h";
  case T(3): return "k";
  case T(6): return "M";
  case T(9): return "G";
  case T(12): return "T";
  case T(15): return "P";
  case T(18): return "E";
  case T(21): return "Z";
  case T(24): return "Y";
  case T(27): return "R";
  case T(30): return "Q";
  default: return std::nullopt;
  }
}

/// @brief Returns the binary prefix for the specified power
/// @tparam T The (unsigned integral) type of the values that are passed as this function's parameters
/// @param power The power to express as a unit prefix
/// @return The unit prefix for the specified power, or nullopt if no prefix applies to it
/// @remark Return values (that are not nullopt) include the 'i' indicator that it's a binary (as opposed to SI) prefix.
template <std::unsigned_integral T>
std::optional<std::string> BinaryPrefix(T power) {
  // Binary prefixes are only defined for powers that are multiples of 1024 (i.e. 2^10): see https://en.wikipedia.org/wiki/Binary_prefix .
  // Concretely, we don't want to return "dai" (e.g. decabytes) or "hi" (e.g. hectobytes).
  if (power % 10U != 0) {
    return std::nullopt;
  }

  // Binary (base-2) powers of 10 correspond with decimal (base-10) powers of 3:
  // - 2^10 =       1024 corresponds with 10^3 =       1000
  // - 2^20 =    1048576 corresponds with 10^6 =    1000000
  // - 2^30 = 1073741824 corresponds with 10^9 = 1000000000
  // - ... and so on
  auto si = SiPrefix(power / 10U * 3U);

  if (si.has_value()) {
    return *si + 'i';
  }
  return std::nullopt;
}

}
