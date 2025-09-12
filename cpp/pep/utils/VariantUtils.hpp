#pragma once

#include <type_traits>
#include <variant>

namespace pep::variant_utils {
namespace detail {

/// `true` if `T` is a class specialization of `std::variant` and `false` otherwise
template <typename T>
inline constexpr bool is_variant_v = false;

// specialization for T matching std::variant<Ts...>
template <typename... Ts>
inline constexpr bool is_variant_v<std::variant<Ts...>> = true;

} // namespace detail

/// Accepts class specializations of `std::variant`
template <typename T>
concept VariantType = detail::is_variant_v<T>;

static_assert(!VariantType<int>);
static_assert(VariantType<std::variant<int>>);
static_assert(VariantType<std::variant<>>); // edge case

namespace detail {

/// `std:true_type` if `T` is one of the alternatives of `Variant` and `std::false_type` otherwise
///
/// @note Requires _partial_ specialization, so this needs to be a class template
template <typename T, VariantType V>
struct is_alternative_of;

// partial specialization for V matching std::variant<Ts...> (which is exactly VariantType)
template <typename T, typename... Ts>
struct is_alternative_of<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};

} // namespace detail

/// Accepts types that are an alternatives of a specialization of `std::variant`
///
/// @example
///   ```
///   template <AlternativeOf<MyVariant> T>
///   std::size_t toString(T alternative) { return alternative.size(); }
///   ```
template <typename T, typename VariantType> // params seem backwards but works as `AlternativeOf<VariantT> AlternativeT`
concept AlternativeOf = detail::is_alternative_of<T, VariantType>::value;

static_assert(!AlternativeOf<int, std::variant<bool, float>>);
static_assert(AlternativeOf<int, std::variant<bool, int, float>>);
static_assert(!AlternativeOf<int, std::variant<>>); // edge case

/// Generates a struct from a list of lambda functions, which can be used as std::variant visitor.
///
/// It works by defining a struct that inherits and exposes `operator()` from each template argument.
/// @see https://en.cppreference.com/w/cpp/utility/variant/visit2
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

} // namespace pep::variant_utils
