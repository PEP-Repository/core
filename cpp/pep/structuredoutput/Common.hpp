#pragma once

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

enum class Format { yaml, json };

struct DisplayConfig final {
  // Avoiding boilerplate code that would be needed to support bitwise ops on an enum type.
  struct Flags final {
    using T = unsigned;
    constexpr static T printHeaders = 1 << 1;
    constexpr static T printGroups = 1 << 2;
    constexpr static T printUserGroups = 1 << 3;
    constexpr static T printUsers = 1 << 4;

    Flags() = delete;
  };

  Flags::T flags = Flags::printHeaders | Flags::printGroups | Flags::printUserGroups | Flags::printUsers;
  int indent = 0; ///< How many levels the output should be indented by.
  Format preferredFormat = Format::yaml;
};

inline std::string indentations(int i) {
  i = std::max(i, 0); // Treat negative as 0
  // Brace initialization would result in different/unwanted result here.
  return std::string(static_cast<std::size_t>(2 * i), ' ');
}

} // namespace pep::structuredOutput

