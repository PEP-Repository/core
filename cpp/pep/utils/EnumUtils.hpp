#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <utility>

namespace pep {
inline namespace enumUtils { // to allow selective import of just these definitions

template <FlagEnum T>
constexpr T operator~(const T flags) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(~std::to_underlying(flags) & std::to_underlying(T::All));
}

template <FlagEnum T>
constexpr T operator| (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template <FlagEnum T>
constexpr T operator& (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template <FlagEnum T>
constexpr T operator^ (const T lhs, const T rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<T>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
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
