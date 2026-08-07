#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <string_view>

namespace pep::structuredOutput {

namespace queryKeys {

// ama query keys
constexpr QueryKey columns{"columns", "Columns"}; // input for --include flag
constexpr QueryKey columnGroup{"column-group", "Column Group"};
constexpr QueryKey columnGroups{"column-groups", "Column Groups"}; // input for --include flag
constexpr QueryKey columnGroupAccessRules{"column-group-access-rules", "Column Group Access Rules"}; // input for --include flag
constexpr QueryKey participantGroup{"participant-group", "Participant Group"};
constexpr QueryKey participantGroups{"participant-groups", "Participant Groups"}; // input for --include flag
constexpr QueryKey participantGroupAccessRules{"participant-group-access-rules", "Participant Group Access Rules"}; // input for --include flag
constexpr QueryKey mode{"mode", "Mode"};

} // namespace queryKeys

enum class PEP_ATTRIBUTE_FLAG_ENUM AmaQueryFlags {
  None = 0,
  PrintColumns = 0b00001,
  PrintColumnGroups = 0b00010,
  PrintColumnGroupAccessRules = 0b00100,
  PrintParticipantGroups = 0b01000,
  PrintParticipantGroupAccessRules = 0b10000,
  All = 0b11111,
};

Tree TreeFrom(const pep::AmaQueryResponse& res, const QueryDisplayConfig<AmaQueryFlags>& config);

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::AmaQueryFlags)