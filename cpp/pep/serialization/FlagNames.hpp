#pragma once

#include <pep/serialization/Error.hpp>
#include <pep/utils/FlagNames.hpp>
#include <pep/utils/Log.hpp>

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace pep {

/// Maps the \p str to a value via \p table
/// \throws Error if \p str is not in \p table, using \p errorFormat for the error message
template <FlagEnum T, std::size_t N>
T ParseFlagName(
    const std::string_view str,
    const FlagNameTable<T, N>& table,
    const std::format_string<std::string> errorFormat) {
  for (const auto& [flag, flagName] : table) {
    if (str == flagName) { return flag; }
  }
  throw Error(std::format(errorFormat, Logging::Escape(std::string(str))));
}

/// Maps the \p strings to values via \p table and returns the combination of those values
/// \throws Error if one the \p strings is not in the \p table, using \p errorFormat for the error message
template <FlagEnum T, std::size_t N>
T ParseFlagNames(
    const std::span<const std::string> strings,
    const FlagNameTable<T, N>& table,
    const std::format_string<std::string> errorFormat) {
  auto result = T::None;
  for (const auto& name : strings) {
    result |= ParseFlagName(name, table, errorFormat);
  }
  return result;
}

} // namespace pep
