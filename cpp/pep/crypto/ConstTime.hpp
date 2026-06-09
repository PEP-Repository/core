#pragma once

#include <functional>
#include <numeric>
#include <ranges>

namespace pep::const_time {

/// Check if all elements are zero in constant time (only depending on the length)
bool IsZero(const std::ranges::input_range auto& data) {
  using namespace std::ranges;
  const auto zero = range_value_t<decltype(data)>{};
  return std::reduce(begin(data), end(data),
                     zero, std::bit_or{}) == zero;
}

bool IsEqual(const std::ranges::input_range auto& lhs, const std::ranges::input_range auto& rhs)
requires (std::same_as<std::ranges::range_value_t<decltype(lhs)>, std::ranges::range_value_t<decltype(rhs)>>) {
  using namespace std::ranges;
  using Type = range_value_t<decltype(lhs)>;
  Type inequalBits{};
  auto lhsIt = begin(lhs);
  auto rhsIt = begin(rhs);
  for (; lhsIt != end(lhs) && rhsIt != end(rhs); ++lhsIt, ++rhsIt) {
    // Avoid using `==`/`!=`, which may lead to branching.
    // `static_cast` is to revert integer promotion.
    inequalBits |= static_cast<Type>(*lhsIt ^ *rhsIt);
  }
  if (lhsIt != end(lhs) || rhsIt != end(rhs)) {
    return false; // Sizes differ
  }
  return inequalBits == Type{};
}

} // namespace pep::const_time
