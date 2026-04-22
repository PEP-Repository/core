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
    PrintHeaders = 1 << 1,
    PrintGroups = 1 << 2,
    PrintUserGroups = 1 << 3,
    PrintUsers = 1 << 4,
    All = (1 << 5) - 1,
  };

  static_assert(FlagEnum<Flags>);

  Flags flags = Flags::PrintHeaders | Flags::PrintGroups | Flags::PrintUserGroups | Flags::PrintUsers;
  int indent = 0; ///< How many levels the output should be indented by.
  Format preferredFormat = Format::Yaml;
};

inline std::string indentations(int i) {
  i = std::max(i, 0); // Treat negative as 0
  // Brace initialization would result in different/unwanted result here.
  return std::string(static_cast<std::size_t>(2 * i), ' ');
}

} // namespace pep::structuredOutput

