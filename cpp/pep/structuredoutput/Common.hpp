#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <string>
#include <string_view>

namespace pep::structuredOutput {

enum class WhitespaceFormat {
  Compact,   // No indentation (single line)
  TwoSpaces, // 2 spaces per indentation level
  FourSpaces // 4 spaces per indentation level
};

namespace queryKeys {
struct QueryKey final {
  std::string_view simple;
  std::string_view descriptive;
};

constexpr QueryKey userGroup{"user-group", "User Group"};
constexpr QueryKey userGroups{"user-groups", "User Groups"}; // input for --include flag

// user query keys
constexpr QueryKey groupsPerUser{"user-groups-per-user", "User Groups Per User"}; // input for --include flag
constexpr QueryKey users{"users", "Users"}; // input for --include flag
constexpr QueryKey displayId{"display-id", "Display ID"};
constexpr QueryKey primaryId{"primary-id", "Primary ID"};
constexpr QueryKey otherIdentifiers{"other-identifiers", "Other User Identifiers"};
constexpr QueryKey maxAuthValidity{"max-token-validity", "Maximum Token Validity"};
constexpr QueryKey name{"name", "Name"};

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

  enum class Format {
    Yaml,
    Json
  };

struct UserQueryDisplayConfig final {
  enum class PEP_ATTRIBUTE_FLAG_ENUM Flags {
    None = 0,
    PrintUserGroups = 0b001,
    PrintUserGroupsForUsers = 0b010,
    PrintUsers = 0b100,
    All = 0b111,
  };

  Flags flags = Flags::All;
  Format format = Format::Yaml;
  bool useDescriptiveKeys = true; ///< Controls whether to use descriptive keys ("Display ID") vs simple keys ("display-id") for all fields
};

struct AmaQueryDisplayConfig final {
  enum class PEP_ATTRIBUTE_FLAG_ENUM Flags {
    None = 0,
    PrintColumns = 0b00001,
    PrintColumnGroups = 0b00010,
    PrintColumnGroupAccessRules = 0b00100,
    PrintParticipantGroups = 0b01000,
    PrintParticipantGroupAccessRules = 0b10000,
    All = 0b11111,
  };

  Flags flags = Flags::All;
  Format format = Format::Yaml;
  bool useDescriptiveKeys = true; ///< Controls whether to use descriptive keys ("Display ID") vs simple keys ("display-id") for all fields
};

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::UserQueryDisplayConfig::Flags)
PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::AmaQueryDisplayConfig::Flags)