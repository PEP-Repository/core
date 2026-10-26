#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pep {

/// Access modes for participant groups and their participants
enum class PEP_ATTRIBUTE_FLAG_ENUM SubjectAccessModes {
  None      = 0b00,
  Access    = 0b01,
  Enumerate = 0b10,
  All = Access | Enumerate,
};

} // namespace pep

PEP_MARK_AS_FLAG_ENUM_TYPE(::pep::SubjectAccessModes)

namespace pep {

/// Names of the individual modes that are set in \p modes, in declaration
/// order.
std::vector<std::string> ToIndividualStrings(SubjectAccessModes modes);

/// Names of all modes that are set in \p modes, joined into a single string.
std::string ToSingleString(SubjectAccessModes modes,
                           std::string_view separator = "|");

/// Interpret \p name (like "access") as an individual mode.
/// \throws Error if \p name doesn't denote a known mode.
SubjectAccessModes ParseSubjectAccessMode(std::string_view name);

/// Interpret \p names as a combination of modes.
/// \throws Error if one of \p names doesn't denote a known mode.
SubjectAccessModes ParseSubjectAccessModes(std::span<const std::string> names);

} // namespace pep
