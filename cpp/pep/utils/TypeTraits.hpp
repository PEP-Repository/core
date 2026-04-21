#pragma once

#include <type_traits>

namespace pep {

template<typename T>
concept ByteLike = sizeof(T) == 1;

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept FlagEnum = requires {
  Enum<T>;
  { T::None };
  { T::All };
};

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

}
