#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <string>
#include <string_view>

namespace pep::structuredOutput {
namespace queryKeys {
struct QueryKey final {
  std::string_view simple;
  std::string_view descriptive;
};

constexpr QueryKey userGroups{"all-user-group", "All User Groups"};
constexpr QueryKey groupsPerUser{"groups-per-user", "User Groups per Interactive User"};
constexpr QueryKey users{"all-user", "All Interactive Users"};
constexpr QueryKey displayId{"displayId", "Display ID"};
constexpr QueryKey primaryId{"primaryId", "Primary ID"};
constexpr QueryKey otherIdentifiers{"otherIdentifiers", "Other User Identifiers"};
constexpr QueryKey groups{"groups", "User Groups"};
constexpr QueryKey maxAuthValidity{"maxAuthValidity", "Maximum Token Validity"};
constexpr QueryKey name{"name", "Name"};

} // namespace queryKeys

  enum class Format {
    Yaml,
    Json
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

inline std::string indentations(int i) {
  i = std::max(i, 0); // Treat negative as 0
  // Brace initialization would result in different/unwanted result here.
  return std::string(static_cast<std::size_t>(2 * i), ' ');
}

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::UserQueryDisplayConfig::Flags)
