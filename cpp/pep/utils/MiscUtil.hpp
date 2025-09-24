#pragma once

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
#define PEP_WrapOverloadedFunction(fun) \
  ([](auto&&... args) -> decltype(auto) { \
    return (fun)(std::forward<decltype(args)>(args)...); \
  })

}
