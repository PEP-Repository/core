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

// user query keys
constexpr QueryKey userGroups{"all-user-group", "All User Groups"};
constexpr QueryKey groupsPerUser{"groups-per-user", "User Groups per Interactive User"};
constexpr QueryKey users{"all-user", "All Interactive Users"};
constexpr QueryKey displayId{"displayId", "Display ID"};
constexpr QueryKey primaryId{"primaryId", "Primary ID"};
constexpr QueryKey otherIdentifiers{"otherIdentifiers", "Other User Identifiers"};
constexpr QueryKey groups{"groups", "User Groups"};
constexpr QueryKey maxAuthValidity{"maxAuthValidity", "Maximum Token Validity"};
constexpr QueryKey name{"name", "Name"};

// ama query keys
constexpr QueryKey columns{"columns", "Columns"};
constexpr QueryKey columnGroups{"column-groups", "Column Groups"};
constexpr QueryKey columnGroupAccessRules{"column-group-access-rules", "Column Group Access Rules"};
constexpr QueryKey participantGroups{"participant-groups", "Participant Groups"};
constexpr QueryKey participantGroupAccessRules{"participant-group-access-rules", "Participant Group Access Rules"};
constexpr QueryKey columnGroup{"columnGroup", "Column Group"};
constexpr QueryKey mode{"mode", "Mode"};
constexpr QueryKey participantGroup{"participantGroup", "Participant Group"};
constexpr QueryKey userGroup{"userGroup", "User Group"};

} // namespace queryKeys

  enum class Format {
    Yaml,
    Json,
    Text
  };

struct UserQueryDisplayConfig final {
  enum class PEP_ATTRIBUTE_FLAG_ENUM Flags {
    None = 0,
    PrintHeaders = 0b0001,
    PrintUserGroups = 0b0010,
    PrintUserGroupsForUsers = 0b0100,
    PrintUsers = 0b1000,
    All = 0b1111,
  };

  Flags flags = Flags::All;
  Format preferredFormat = Format::Yaml;
  bool useDescriptiveHeaders = true; ///< Controls whether to use descriptive keys ("Display ID") vs simple keys ("displayId") for all fields
};

struct AmaQueryDisplayConfig final {
  enum class PEP_ATTRIBUTE_FLAG_ENUM Flags {
    None = 0,
    PrintHeaders = 0b00001,
    PrintColumns = 0b00010,
    PrintColumnGroups = 0b00100,
    PrintColumnGroupAccessRules = 0b01000,
    PrintParticipantGroups = 0b10000,
    PrintParticipantGroupAccessRules = 0b100000,

    All = 0b111111,
  };

  Flags flags = Flags::All;
  Format preferredFormat = Format::Yaml;
  bool useDescriptiveHeaders = true; ///< Controls whether to use descriptive keys ("Display ID") vs simple keys ("displayId") for all fields
};

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::UserQueryDisplayConfig::Flags)
PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::AmaQueryDisplayConfig::Flags)