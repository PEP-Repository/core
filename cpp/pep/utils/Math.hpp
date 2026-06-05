#pragma once

#include <concepts>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace pep {

/// Generic version of \c std::abs
template <typename T>
[[nodiscard]] constexpr auto Abs(T&& v) {
  return v < T{/*default*/} ? -std::forward<T>(v) : std::forward<T>(v);
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr T CeilDiv(const T dividend, const T divisor) noexcept {
  auto quotient = dividend / divisor;
  const auto remainder = dividend % divisor;
  if (remainder) {
    ++quotient;
  }
  return quotient;
}

/// Narrowing integer cast, alternative for \c static_cast
/// \throws std::range_error if \p from does not fit in \c To
template <std::integral To>
constexpr To CheckedCast(std::integral auto from) {
  if (std::cmp_less(from, std::numeric_limits<To>::min())
    || std::cmp_greater(from, std::numeric_limits<To>::max())) {
    throw std::range_error("CheckedCast: number out of range");
  }
  return static_cast<To>(from);
}

/// @brief Calculates the logarithm of an (unsigned) integral value.
/// @tparam T The unsigned integral type
/// @param value The operand (value)
/// @param base The base for the logarithm
/// @return The value's logarithm in the specified base, or std::nullopt if the value has no (exact) integral logarithm in the specified base
template <std::unsigned_integral T>
constexpr std::optional<T> IntegralLog(T value, T base) {
  if (value == 0U) {
    return std::nullopt;
  }

  T result = 0U;
  while (value != 1U) {
    if (value % base != 0U) {
      return std::nullopt;
    }
    ++result;
    value /= base;
  }
  return result;
}


}

