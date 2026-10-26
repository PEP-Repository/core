#include <pep/ticketing/SubjectAccessModes.hpp>

#include <pep/serialization/FlagNames.hpp>
#include <pep/utils/FlagNames.hpp>

namespace pep {
namespace {

constexpr auto subjectAccessModeNames = FlagNameTable<SubjectAccessModes, 2>{{
    {SubjectAccessModes::Access, "access"},
    {SubjectAccessModes::Enumerate, "enumerate"},
}};

constexpr std::string_view subjectAccessModeErrorFormat = "Unknown subject access mode {}";

} // anonymous namespace

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
