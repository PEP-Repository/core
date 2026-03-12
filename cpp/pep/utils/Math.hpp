#pragma once

#include <concepts>
#include <limits>
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

namespace detail {
// Avoids <stdexcept> header inclusion
/// \throws std::range_error Always
[[noreturn]] void CheckedCastThrow();
}

/// Narrowing integer cast, alternative for \c static_cast
/// \throws std::range_error if \p from does not fit in \c To
template <std::integral To>
constexpr To CheckedCast(std::integral auto from) {
  if (std::cmp_less(from, std::numeric_limits<To>::min())
    || std::cmp_greater(from, std::numeric_limits<To>::max())) {
    detail::CheckedCastThrow();
  }
  return static_cast<To>(from);
}
  
}

