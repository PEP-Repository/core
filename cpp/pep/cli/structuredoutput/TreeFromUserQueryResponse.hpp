#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp> // IWYU pragma: export (operators for flag enums)
#include <pep/utils/TypeTraits.hpp> // for PEP_MARK_AS_FLAG_ENUM_TYPE

namespace pep::structuredOutput {

namespace queryKeys {

// user query keys
constexpr QueryKey groupsPerUser{"user-groups-per-user", "User Groups Per User"}; // input for --include flag
constexpr QueryKey users{"users", "Users"}; // input for --include flag
constexpr QueryKey displayId{"display-id", "Display ID"};
constexpr QueryKey primaryId{"primary-id", "Primary ID"};
constexpr QueryKey otherIdentifiers{"other-identifiers", "Other User Identifiers"};
constexpr QueryKey maxAuthValidity{"max-token-validity", "Maximum Token Validity"};
constexpr QueryKey expiration{"expiration", "Expiration"};

} // namespace queryKeys

enum class PEP_ATTRIBUTE_FLAG_ENUM UserQueryFlags {
  None = 0,
  PrintUserGroups = 0b001,
  PrintUserGroupsForUsers = 0b010,
  PrintUsers = 0b100,
  All = 0b111,
};

Tree TreeFrom(const pep::UserQueryResponse& res, const QueryDisplayConfig<UserQueryFlags>& config);

} // namespace pep::structuredOutput

PEP_MARK_AS_FLAG_ENUM_TYPE(pep::structuredOutput::UserQueryFlags)