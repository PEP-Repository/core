#include <pep/structuredoutput/FormatFlags.hpp>

#include <pep/utils/FlagNames.hpp>

namespace pep::structuredOutput {

constexpr auto formatFlagNames = std::array<std::pair<FormatFlags, std::string_view>, 3>{{
    {FormatFlags::Csv, "csv"},
    {FormatFlags::Json, "json"},
    {FormatFlags::Yaml, "yaml"},
}};
static_assert(FormatFlags::All == (FormatFlags::Csv | FormatFlags::Json | FormatFlags::Yaml),
    "every format must be in formatFlagNames");

std::vector<std::string> ToIndividualStrings(FormatFlags flags) {
  return pep::ToIndividualStrings(flags, formatFlagNames);
}

std::string ToSingleString(FormatFlags flags, std::string_view separator) {
  return pep::ToSingleString(flags, formatFlagNames, separator);
}

} // namespace pep::structuredOutput
