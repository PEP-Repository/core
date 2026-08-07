#pragma once

#include <functional>
#include <numeric>
#include <ranges>
#include <string>

// For implementors:
// Constant-time functions cannot use branching (if, ternary, short-circuit logic, ...) on secrets.
// They can also not index memory depending on secrets (lookup tables),
// because of caching (reads accessing memory that wasn't cached take more time).
// They can also not use operations that may not be constant time, like division.
// See https://github.com/veorq/cryptocoding.
// Smart compilers may still mess with this, however:
// see https://discourse.llvm.org/t/rfc-constant-time-coding-support/87781 and https://eprint.iacr.org/2025/435.

/// Constant-time functions for operations on secrets.
///
/// The running time of these functions does not depend on the value of a secret input.
/// This protects against timing attacks.
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

/// Convert \p bytes to hexadecimal in constant time (only depending on the length).
/// \returns Hexadecimal string. Length is always a multiple of 2.
std::string ToHex(std::string_view bytes);

/// Convert \p hex to bytes from hexadecimal in constant time (only depending on the length).
/// \throws std::invalid_argument for length not divisible by 2.
/// \throws std::invalid_argument for invalid hex character. For invalid characters, the execution time may depend on the value.
/// \returns String of bytes.
std::string FromHex(std::string_view hex);

} // namespace pep::const_time
