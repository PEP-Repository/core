#pragma once

#include <pep/utils/EnumUtils.hpp>
#include <string>
#include <string_view>

namespace pep::structuredOutput {
namespace stringConstants {
struct Strings final {
  std::string_view option;
  std::string_view descriptive;
};

constexpr Strings userGroups{"all-user-group", "All User Groups"};

constexpr Strings groupsPerUser{"groups-per-user", "User Groups per Interactive User"};

constexpr Strings users{"all-user", "All Interactive Users"};

constexpr std::string_view displayIdKey{"display id"};

constexpr std::string_view primaryIdKey{"primary id"};

constexpr std::string_view otherIdentifiersKey{"other user identifiers"};

constexpr std::string_view groupsKey{"user groups"};

constexpr std::string_view maxAuthValidityKey{"max auth valid time"};

} // namespace stringConstants

enum class Format { Yaml, Json };

struct DisplayConfig final {
  enum class Flags {
    None = 0,
    PrintHeaders = 0b0001,
    PrintGroups = 0b0010,
    PrintUserGroups = 0b0100,
    PrintUsers = 0b1000,
    All = 0b1111,
  };

  Flags flags = Flags::All;
  int indent = 0; ///< How many levels the output should be indented by.
  Format preferredFormat = Format::Yaml;
};

inline std::string indentations(int i) {
  i = std::max(i, 0); // Treat negative as 0
  // Brace initialization would result in different/unwanted result here.
  return std::string(static_cast<std::size_t>(2 * i), ' ');
}

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::DisplayConfig::Flags)

