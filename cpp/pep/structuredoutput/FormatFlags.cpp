#include <pep/structuredoutput/FormatFlags.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep::structuredOutput {

std::vector<std::string> ToIndividualStrings(FormatFlags flags) {
  std::vector<std::string> strs{};
  if (Contains(flags, FormatFlags::csv)) strs.emplace_back("csv");
  if (Contains(flags, FormatFlags::json)) strs.emplace_back("json");
  if (Contains(flags, FormatFlags::yaml)) strs.emplace_back("yaml");
  return strs;
}

std::string ToSingleString(FormatFlags flags, std::string_view separator) {
  if (flags == FormatFlags::none) { return "none"; }
  if (Contains(flags, FormatFlags::all)) { return "all"; }
  return boost::join(ToIndividualStrings(flags), separator);
}

} // namespace pep::structuredOutput
