#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/TypeTraits.hpp>

#include <vector>
#include <string>
#include <string_view>

namespace pep::structuredOutput {

/// Enum flags for all formats in pep::structuredOutput
enum class PEP_ATTRIBUTE_FLAG_ENUM FormatFlags {
  none = 0b000,
  csv = 0b001,
  json = 0b010,
  yaml = 0b100,
  all = 0b111,
};

constexpr inline FormatFlags operator~(const FormatFlags flags) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(~ToUnderlying(flags) & ToUnderlying(FormatFlags::all));
}
static_assert(ToUnderlying(~FormatFlags::none) == ToUnderlying(FormatFlags::all), "none is the bitwise inversion of all");
static_assert(ToUnderlying(~FormatFlags::all) == ToUnderlying(FormatFlags::none), "all is the bitwise inversion of none");

constexpr inline FormatFlags operator| (const FormatFlags lhs, const FormatFlags rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
}

constexpr inline FormatFlags operator& (const FormatFlags lhs, const FormatFlags rhs) noexcept {
  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) all (bitwise) combinations of flags are valid
  return static_cast<FormatFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
}

constexpr inline FormatFlags& operator|= (FormatFlags& lhs, const FormatFlags rhs) noexcept {
  return lhs = (lhs | rhs);
}

constexpr inline FormatFlags& operator&= (FormatFlags& lhs, const FormatFlags rhs) noexcept {
  return lhs = (lhs & rhs);
}

/// Test if \p lhs contains at least all the flags of \p rhs
constexpr inline bool Contains(const FormatFlags haystack, const FormatFlags needle) noexcept { return (haystack & needle) == needle; }
static_assert(Contains(FormatFlags::none, FormatFlags::none), "Test(x, FormatFlags::none) is always true");

std::vector<std::string> ToIndividualStrings(FormatFlags);
std::string ToSingleString(FormatFlags, std::string_view separator = "|");

} // namespace pep::structuredOutput
