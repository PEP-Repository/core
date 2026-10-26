#include <pep/ticketing/SubjectAccessModes.hpp>

#include <pep/serialization/FlagNames.hpp>
#include <pep/utils/FlagNames.hpp>

namespace pep {

constexpr auto subjectAccessModeNames = std::array<std::pair<SubjectAccessModes, std::string_view>, 2>{{
    {SubjectAccessModes::Access, "access"},
    {SubjectAccessModes::Enumerate, "enumerate"},
}};
constexpr std::string_view subjectAccessModeErrorFormat = "Unknown subject access mode {}";

std::vector<std::string> ToIndividualStrings(const SubjectAccessModes modes) {
  return ToIndividualStrings(modes, subjectAccessModeNames);
}

std::string ToSingleString(const SubjectAccessModes modes, const std::string_view separator) {
  return ToSingleString(modes, subjectAccessModeNames, separator);
}

SubjectAccessModes ParseSubjectAccessMode(const std::string_view str) {
  return ParseFlagName(str, subjectAccessModeNames, subjectAccessModeErrorFormat);
}

SubjectAccessModes ParseSubjectAccessModes(const std::span<const std::string> names) {
  return ParseFlagNames(names, subjectAccessModeNames, subjectAccessModeErrorFormat);
}

} // namespace pep
