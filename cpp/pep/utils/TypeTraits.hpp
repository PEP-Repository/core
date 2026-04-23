#pragma once

#include <type_traits>

namespace pep {
namespace detail {

template <typename T> inline constexpr bool MARKED_AS_FLAG_ENUM_TYPE = false;

}

template<typename T>
concept ByteLike = sizeof(T) == 1;

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept FlagEnumCanditate = requires {
  Enum<T>;
  { T::None };
  { T::All };
};

template <typename T>
concept FlagEnum = FlagEnumCanditate<T> && detail::MARKED_AS_FLAG_ENUM_TYPE<T>;

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

#define PEP_MARK_AS_FLAG_ENUM_TYPE(T) \
  static_assert(::pep::FlagEnumCanditate<T>); \
  template <> constexpr inline bool ::pep::detail::MARKED_AS_FLAG_ENUM_TYPE<T> = true;

