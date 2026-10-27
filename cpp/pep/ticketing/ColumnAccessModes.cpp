#include <pep/ticketing/ColumnAccessModes.hpp>

#include <pep/serialization/FlagNames.hpp>
#include <pep/utils/FlagNames.hpp>

namespace pep {
namespace {

constexpr auto columnAccessModeNames = std::array<std::pair<ColumnAccessModes, std::string_view>, 5>{{
    {ColumnAccessModes::Read, "read"},
    {ColumnAccessModes::ReadMeta, "read-meta"},
    {ColumnAccessModes::Write, "write"},
    {ColumnAccessModes::WriteMeta, "write-meta"},
    {ColumnAccessModes::_bogus_AmaTranscryptionOnly, "enumerate"}, // still maps to old name for backward compatibility
}};

constexpr std::string_view columnAccessModeErrorFormat = "Unknown column access mode {}";

} // anonymous namespace

std::vector<std::string> ToIndividualStrings(const ColumnAccessModes modes) {
  return ToIndividualStrings(modes, columnAccessModeNames);
}

std::string ToSingleString(const ColumnAccessModes modes, const std::string_view separator) {
  return ToSingleString(modes, columnAccessModeNames, separator);
}

ColumnAccessModes ParseColumnAccessMode(const std::string_view str) {
  return ParseFlagName(str, columnAccessModeNames, columnAccessModeErrorFormat);
}

ColumnAccessModes ParseColumnAccessModes(const std::span<const std::string> names) {
  return ParseFlagNames(names, columnAccessModeNames, columnAccessModeErrorFormat);
}

} // namespace pep
