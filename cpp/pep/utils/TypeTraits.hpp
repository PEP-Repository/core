#pragma once

#include <type_traits>

namespace pep {
namespace detail {

/// Mechanism to tracks if PEP_MARK_AS_FLAG_ENUM_TYPE was called on T
template <typename T> inline constexpr bool MARKED_AS_FLAG_ENUM_TYPE = false;

}

template<typename T>
concept ByteLike = sizeof(T) == 1;

template <typename T>
concept Enum = std::is_enum_v<T>;

/// Concept that checks if a type matches all requirements of a FlagEnum
///
/// It requires that T is a scoped enum for which `None` and `All` are defined and
/// that the underlying value of `None` is zero.
template <typename T>
concept FlagEnumCandidate = requires {
  requires Enum<T>;
  { T::None };
  { T::All };
} && (T::None == static_cast<T>(0));

/// Scoped enum that was explicitly marked with `PEP_MARK_AS_FLAG_ENUM_TYPE`
///
/// Implies `FlagEnumCandidate<T>`
template <typename T>
concept FlagEnum = FlagEnumCandidate<T> && detail::MARKED_AS_FLAG_ENUM_TYPE<T>;

template<typename T, typename Ref>
using CopyConstness = std::conditional_t<std::is_const_v<Ref>, const T, T>;

//XXX Replace by auto(v) in C++23
[[nodiscard]] auto decay_copy(auto v) { return v; }

// See https://stackoverflow.com/a/70130881
namespace detail {
template<template<typename...> typename Template, typename... Args>
void DerivedFromSpecializationImpl(const Template<Args...>&);
}

/// Usage:
/// \code
/// void myFun(DerivedFromSpecialization<std::chrono::duration> auto&& somethingInheritingFromDuration);
/// \endcode
template <typename T, template <typename...> typename Template>
concept DerivedFromSpecialization = requires(const T& t) {
  detail::DerivedFromSpecializationImpl<Template>(t);
};

} // namespace pep

/// Marks T as a `FlagEnum`, enabling `FlagEnum`-constrained functions and bitwise operators for T.
///
/// Requires `FlagEnumCandidate<T>`
///
/// @warning This macro must be invoked from the `::pep` namespace or global namespace;
///          it does not work from nested namespaces within `::pep`.
#define PEP_MARK_AS_FLAG_ENUM_TYPE(T) \
  static_assert(::pep::FlagEnumCandidate<T>); \
  template <> constexpr inline bool ::pep::detail::MARKED_AS_FLAG_ENUM_TYPE<T> = true;

