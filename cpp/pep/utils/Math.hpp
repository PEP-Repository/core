#pragma once

#include <concepts>
#include <utility>

namespace pep {

/// Generic version of \c std::abs
template <typename T>
[[nodiscard]] constexpr auto Abs(T&& v) {
  return v < T{/*default*/} ? -std::forward<T>(v) : std::forward<T>(v);
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr T CeilDiv(const T divident, const T divisor) noexcept {
  auto quotient = divident / divisor;
  const auto remainder = divident % divisor;
  if (remainder) {
    ++quotient;
  }
  return quotient;
}
  
}

