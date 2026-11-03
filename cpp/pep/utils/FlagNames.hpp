#pragma once

#include <pep/utils/EnumUtils.hpp>

#include <boost/algorithm/string/join.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pep {

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

} // namespace pep
