#pragma once

#include <pep/utils/TypeTraits.hpp>

namespace pep {
inline namespace enumUtils { // to allow selective import of just these definitions

//XXX Replace by std::to_underlying in C++23
[[nodiscard]] constexpr auto ToUnderlying(Enum auto v) noexcept {
  return static_cast<std::underlying_type_t<decltype(v)>>(v);
}

template <FlagEnum T>
constexpr T operator~(const T flags) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(~ToUnderlying(flags) & ToUnderlying(T::All));
}

template <FlagEnum T>
constexpr T operator| (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(ToUnderlying(lhs) | ToUnderlying(rhs));
}

template <FlagEnum T>
constexpr T operator& (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(ToUnderlying(lhs) & ToUnderlying(rhs));
}

template <FlagEnum T>
constexpr T operator^ (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(ToUnderlying(lhs) ^ ToUnderlying(rhs));
}

template <FlagEnum T>
constexpr T& operator|= (T& lhs, const T rhs) noexcept { return lhs = (lhs | rhs); }

template <FlagEnum T>
constexpr T& operator&= (T& lhs, const T rhs) noexcept { return lhs = (lhs & rhs); }

template <FlagEnum T>
constexpr T& operator^= (T& lhs, const T rhs) noexcept { return lhs = (lhs ^ rhs); }

/// Test if \p tested contains at least all the flags of \p required
template <FlagEnum T>
constexpr bool HasFlags(const T tested, const T required) noexcept { return (tested & required) == required; }

/// Returns \p flags if \p condition is true and `T::None` otherwise
template <FlagEnum T>
constexpr T FlagsIf(T flags, bool condition) noexcept { return condition ? flags : T::None; }

} // namespace enumUtils
} // namespace pep
