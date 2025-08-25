#include <pep/structuredoutput/FormatFlags.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep::structuredOutput {

std::vector<std::string> ToIndividualStrings(FormatFlags flags) {
  std::vector<std::string> strs{};
  if (Test(flags, FormatFlags::csv)) strs.emplace_back("csv");
  if (Test(flags, FormatFlags::json)) strs.emplace_back("json");
  if (Test(flags, FormatFlags::yaml)) strs.emplace_back("yaml");
  return strs;
}

std::string ToSingleString(FormatFlags flags, std::string_view separator) {
  if (flags == FormatFlags::none) { return "none"; }
  if (flags == FormatFlags::all) { return "all"; }
  return boost::join(ToIndividualStrings(flags), separator);
}

} // namespace pep::structuredOutput
