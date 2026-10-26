#include <pep/ticketing/SubjectAccessModes.hpp>

#include <pep/ticketing/FlagNames.hpp>

namespace pep {

constexpr auto subjectAccessModeNames = std::array<std::pair<SubjectAccessModes, std::string_view>, 2>{{
    {SubjectAccessModes::Access, "access"},
    {SubjectAccessModes::Enumerate, "enumerate"},
}};
constexpr std::string_view subjectAccessModeErrorFormat = "Unknown subject access mode {}";

std::vector<std::string> ToIndividualStrings(const SubjectAccessModes modes) {
  return detail::ToIndividualStrings(modes, subjectAccessModeNames);
}

std::string ToSingleString(const SubjectAccessModes modes, const std::string_view separator) {
  return detail::ToSingleString(modes, subjectAccessModeNames, separator);
}

SubjectAccessModes ParseSubjectAccessMode(const std::string_view str) {
  return detail::ParseFlagName(str, subjectAccessModeNames, subjectAccessModeErrorFormat);
}

SubjectAccessModes ParseSubjectAccessModes(const std::span<const std::string> names) {
  return detail::ParseFlagNames(names, subjectAccessModeNames, subjectAccessModeErrorFormat);
}

} // namespace pep
