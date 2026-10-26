#include <pep/ticketing/SubjectAccessModes.hpp>

#include <pep/serialization/Error.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>

namespace pep {

constexpr auto subjectAccessModeNames = std::array<std::pair<SubjectAccessModes, std::string_view>, 2>{{
    {SubjectAccessModes::Access, "access"},
    {SubjectAccessModes::Enumerate, "enumerate"},
}};

std::vector<std::string> ToIndividualStrings(const SubjectAccessModes modes) {
  std::vector<std::string> strs{};
  for (const auto& [mode_i, name_i] : subjectAccessModeNames) {
    if (HasFlags(modes, mode_i)) { strs.emplace_back(name_i); }
  }
  return strs;
}

std::string ToSingleString(const SubjectAccessModes modes, const std::string_view separator) {
  if (modes == SubjectAccessModes::None) { return "none"; }
  return boost::join(ToIndividualStrings(modes), separator);
}

SubjectAccessModes ParseSubjectAccessMode(const std::string_view str) {
  for (const auto& [mode, name] : subjectAccessModeNames) {
    if (str == name) { return mode; }
  }
  throw Error("Unknown subject access mode " + Logging::Escape(std::string(str)));
}

SubjectAccessModes ParseSubjectAccessModes(const std::span<const std::string> names) {
  auto result = SubjectAccessModes::None;
  for (const auto& name : names) {
    result |= ParseSubjectAccessMode(name);
  }
  return result;
}

} // namespace pep
