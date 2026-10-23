#include <pep/ticketing/ColumnAccessModes.hpp>

#include <pep/serialization/Error.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>

#include <array>
#include <string_view>

namespace pep {

constexpr auto columnAccessModeNames = std::array<std::pair<ColumnAccessModes, std::string_view>, 5>{{
    {ColumnAccessModes::Read, "read"},
    {ColumnAccessModes::ReadMeta, "read-meta"},
    {ColumnAccessModes::Write, "write"},
    {ColumnAccessModes::WriteMeta, "write-meta"},
    {ColumnAccessModes::_bogus_AmaTranscryptionOnly, "enumerate"},
}};

std::vector<std::string> ToIndividualStrings(const ColumnAccessModes modes) {
  std::vector<std::string> strs{};
  for (const auto& [mode_i, name_i] : columnAccessModeNames) {
    if (HasFlags(modes, mode_i)) { strs.emplace_back(name_i); }
  }
  return strs;
}

std::string ToSingleString(const ColumnAccessModes modes, const std::string_view separator) {
  if (modes == ColumnAccessModes::None) { return "none"; }
  return boost::join(ToIndividualStrings(modes), separator);
}

ColumnAccessModes ParseMode(const std::string_view str) {
  for (const auto& [mode, name] : columnAccessModeNames) {
    if (str == name) { return mode; }
  }
  throw Error("Unknown ticket mode " + Logging::Escape(std::string(str)));
}

ColumnAccessModes ParseModes(const std::span<const std::string> names) {
  auto result = ColumnAccessModes::None;
  for (const auto& name : names) {
    result |= ParseMode(name);
  }
  return result;
}

} // namespace pep
