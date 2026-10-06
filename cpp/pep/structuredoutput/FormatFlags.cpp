#include <pep/structuredoutput/FormatFlags.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep::structuredOutput {

std::vector<std::string> ToIndividualStrings(FormatFlags flags) {
  static_assert(
      FormatFlags::All == (FormatFlags::Csv | FormatFlags::Json | FormatFlags::Yaml),
      "every format must be handled below");
  std::vector<std::string> strs{};
  if (HasFlags(flags, FormatFlags::Csv)) { strs.emplace_back("csv"); }
  if (HasFlags(flags, FormatFlags::Json)) { strs.emplace_back("json"); }
  if (HasFlags(flags, FormatFlags::Yaml)) { strs.emplace_back("yaml"); }
  return strs;
}

std::string ToSingleString(FormatFlags flags, std::string_view separator) {
  if (flags == FormatFlags::None) { return "none"; }
  return boost::join(ToIndividualStrings(flags), separator);
}

} // namespace pep::structuredOutput
