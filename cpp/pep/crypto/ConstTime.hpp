#pragma once

#include <functional>
#include <numeric>
#include <ranges>

namespace pep::const_time {

/// Check if any element is zero in constant time
bool IsZero(const std::ranges::input_range auto& data) {
  const auto zero = std::ranges::range_value_t<decltype(data)>{};
  return std::reduce(std::ranges::begin(data), std::ranges::end(data),
                     zero, std::bit_or{}) == zero;
}

} // namespace pep::const_time
