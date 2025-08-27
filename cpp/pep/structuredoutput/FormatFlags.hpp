#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <pep/utils/Attributes.hpp>

namespace pep::structuredOutput {

/// Enum flags for all formats in pep::structuredOutput
enum class PEP_ATTRIBUTE_FLAG_ENUM FormatFlags {
  none = 0b000,
  csv = 0b001,
  json = 0b010,
  yaml = 0b100,
  all = 0b111,
};

/// Returns the underlying integer value for a set of flags
constexpr inline std::underlying_type_t<FormatFlags> Value(const FormatFlags flags) noexcept {
  return static_cast<std::underlying_type_t<FormatFlags>>(flags);
}

constexpr inline FormatFlags operator~(const FormatFlags flags) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(~Value(flags) & Value(FormatFlags::all));
}
static_assert(Value(~FormatFlags::none) == Value(FormatFlags::all), "none is the bitwise inversion of all");
static_assert(Value(~FormatFlags::all) == Value(FormatFlags::none), "all is the bitwise inversion of none");

constexpr inline FormatFlags operator| (const FormatFlags lhs, const FormatFlags rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(Value(lhs) | Value(rhs));
}

constexpr inline FormatFlags operator& (const FormatFlags lhs, const FormatFlags rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(Value(lhs) & Value(rhs));
}

constexpr inline FormatFlags& operator|= (FormatFlags& lhs, const FormatFlags rhs) noexcept {
  return lhs = (lhs | rhs);
}

constexpr inline FormatFlags& operator&= (FormatFlags& lhs, const FormatFlags rhs) noexcept {
  return lhs = (lhs & rhs);
}

/// Test if \p lhs contains at least all the flags of \p rhs
constexpr inline bool Test(const FormatFlags lhs, const FormatFlags rhs) noexcept { return (lhs & rhs) == rhs; }
static_assert(Test(FormatFlags::none, FormatFlags::none), "Test(x, FormatFlags::none) is always true");

std::vector<std::string> ToIndividualStrings(FormatFlags);
std::string ToSingleString(FormatFlags, std::string_view separator = "|");

} // namespace pep::structuredOutput
