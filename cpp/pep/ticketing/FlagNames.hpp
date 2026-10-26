#pragma once

#include <pep/serialization/Error.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pep::detail {

/// A table that maps each flag of \p T to its textual name, in declaration order.
template <typename T, std::size_t N>
using FlagNameTable = std::array<std::pair<T, std::string_view>, N>;

/// Names of the flags in \p table that are set in \p flags, in table order.
template <FlagEnum T, std::size_t N>
std::vector<std::string> ToIndividualStrings(const T flags, const FlagNameTable<T, N>& table) {
  std::vector<std::string> result{};
  for (const auto& [flag, name] : table) {
    if (HasFlags(flags, flag)) { result.emplace_back(name); }
  }
  return result;
}

/// Returns the names of the flags in \p table that are set in \p flags, joined by \p separator,
/// or "none" if no flags are set.
template <FlagEnum T, std::size_t N>
std::string ToSingleString(const T flags, const FlagNameTable<T, N>& table, const std::string_view separator) {
  if (flags == T::None) { return "none"; }
  return boost::join(ToIndividualStrings(flags, table), separator);
}

/// Maps the \p str to a value via \p table
/// \throws Error if \p str is not in \p table, using \p errorFormat for the error message
template <FlagEnum T, std::size_t N>
T ParseFlagName(const std::string_view str, const FlagNameTable<T, N>& table,
                const std::format_string<std::string> errorFormat) {
  for (const auto& [flag, flagName] : table) {
    if (str == flagName) { return flag; }
  }
  throw Error(std::format(errorFormat, Logging::Escape(std::string(str))));
}

/// Maps the \p strings to values via \p table and returns the combination of those values
/// \throws Error if one the \p strings is not in the \p table, using \p errorFormat for the error message
template <FlagEnum T, std::size_t N>
T ParseFlagNames(const std::span<const std::string> strings, const FlagNameTable<T, N>& table,
                 const std::format_string<std::string> errorFormat) {
  auto result = T::None;
  for (const auto& name : strings) {
    result |= ParseFlagName(name, table, errorFormat);
  }
  return result;
}

} // namespace pep::detail
