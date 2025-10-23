#pragma once

#include <type_traits>
#include <utility>

namespace pep {

template<typename T>
concept ByteLike = sizeof(T) == 1;

template <typename T>
concept Enum = std::is_enum_v<T>;

template<typename T, typename Ref>
using CopyConstness = std::conditional_t<std::is_const_v<Ref>, const T, T>;

//XXX Replace by std::to_underlying in C++23
[[nodiscard]] constexpr auto ToUnderlying(Enum auto v) {
  return static_cast<std::underlying_type_t<decltype(v)>>(v);
}

//XXX Replace by auto(v) in C++23
[[nodiscard]] auto decay_copy(auto v) { return v; }

// See https://stackoverflow.com/a/70130881
namespace detail {
template<template<typename...> typename Template, typename... Args>
void DerivedFromSpecializationImpl(const Template<Args...>&);
}

/// Usage:
/// \code
/// void myFun(DerivedFromSpecialization<std::map> auto someMap);
/// \endcode
template <typename T, template <typename...> typename Template>
concept DerivedFromSpecialization = requires(const T& t) {
  detail::DerivedFromSpecializationImpl<Template>(t);
};


}
