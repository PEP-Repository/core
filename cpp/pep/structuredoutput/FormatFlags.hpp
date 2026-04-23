#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>

#include <vector>
#include <string>
#include <string_view>

namespace pep::structuredOutput {

enum class FormatFlags;

/// Enum flags for all formats in pep::structuredOutput
enum class PEP_ATTRIBUTE_FLAG_ENUM FormatFlags {
  None = 0b000,
  Csv = 0b001,
  Json = 0b010,
  Yaml = 0b100,
  All = 0b111,
};

std::vector<std::string> ToIndividualStrings(FormatFlags);
std::string ToSingleString(FormatFlags, std::string_view separator = "|");

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(::pep::structuredOutput::FormatFlags)
