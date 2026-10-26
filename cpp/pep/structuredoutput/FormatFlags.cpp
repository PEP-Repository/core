#include <pep/structuredoutput/FormatFlags.hpp>

#include <pep/utils/FlagNames.hpp>

namespace pep::structuredOutput {
namespace {

constexpr auto formatFlagNames = FlagNameTable<FormatFlags, 3>{{
    {FormatFlags::Csv, "csv"},
    {FormatFlags::Json, "json"},
    {FormatFlags::Yaml, "yaml"},
}};

} // anonymous namespace

std::vector<std::string> ToIndividualStrings(FormatFlags flags) {
  return pep::ToIndividualStrings(flags, formatFlagNames);
}

std::string ToSingleString(FormatFlags flags, std::string_view separator) {
  return pep::ToSingleString(flags, formatFlagNames, separator);
}

} // namespace pep::structuredOutput
