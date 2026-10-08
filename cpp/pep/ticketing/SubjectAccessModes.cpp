#include <pep/ticketing/SubjectAccessModes.hpp>

#include <pep/serialization/Error.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep {

std::vector<std::string> ToIndividualStrings(const SubjectAccessModes modes) {
  static_assert(
      SubjectAccessModes::All == (SubjectAccessModes::Access | SubjectAccessModes::Enumerate),
      "every mode must be handled below");
  std::vector<std::string> strs{};
  if (HasFlags(modes, SubjectAccessModes::Access)) { strs.emplace_back("access"); }
  if (HasFlags(modes, SubjectAccessModes::Enumerate)) { strs.emplace_back("enumerate"); }
  return strs;
}

std::string ToSingleString(const SubjectAccessModes modes, const std::string_view separator) {
  if (modes == SubjectAccessModes::None) { return "none"; }
  return boost::join(ToIndividualStrings(modes), separator);
}

SubjectAccessModes ParseSubjectAccessMode(const std::string_view name) {
  if (name == "access") { return SubjectAccessModes::Access; }
  if (name == "enumerate") { return SubjectAccessModes::Enumerate; }
  throw Error("Unknown subject access mode " + Logging::Escape(std::string(name)));
}

SubjectAccessModes ParseSubjectAccessModes(const std::span<const std::string> names) {
  auto result = SubjectAccessModes::None;
  for (const auto& name : names) {
    result |= ParseSubjectAccessMode(name);
  }
  return result;
}

} // namespace pep
