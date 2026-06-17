#include <pep/cli/structuredoutput/TreeFromUserQueryResponse.hpp>

#include <nlohmann/json.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/EnumUtils.hpp>

namespace pep::structuredOutput {
namespace {

using json = nlohmann::ordered_json;
using queryKeys::GetKeyName;

}

Tree TreeFrom(const pep::UserQueryResponse& res, const QueryDisplayConfig<UserQueryFlags>& config) {

  const auto printUserGroups = HasFlags(config.flags, UserQueryFlags::PrintUserGroups);
  const auto printUsers = HasFlags(config.flags, UserQueryFlags::PrintUsers);
  const auto printUserGroupsForUsers = HasFlags(config.flags, UserQueryFlags::PrintUserGroupsForUsers);
  const auto useDescriptive = config.useDescriptiveKeys;

  json root = json::object();

  // Build userGroups array
  if (printUserGroups) {
    json groups = json::array();

    for (const auto& group : res.mUserGroups) {
      json item = json::object();
      item.emplace(GetKeyName(queryKeys::name, useDescriptive), group.name_);
      if (group.mMaxAuthValidity) {
        item.emplace(GetKeyName(queryKeys::maxAuthValidity, useDescriptive), pep::chrono::ToString(*group.mMaxAuthValidity));
      }
      groups.push_back(std::move(item));
    }

    root.emplace(GetKeyName(queryKeys::userGroups, useDescriptive), std::move(groups));
  }

  // Build users array
  if (printUsers) {
    json users = json::array();

    for (const auto& user : res.mUsers) {
      json item = json::object();

      if (user.mDisplayId) {
        item.emplace(GetKeyName(queryKeys::displayId, useDescriptive), *user.mDisplayId);
      }

      if (user.mPrimaryId) {
        item.emplace(GetKeyName(queryKeys::primaryId, useDescriptive), *user.mPrimaryId);
      }

      item.emplace(GetKeyName(queryKeys::otherIdentifiers, useDescriptive), user.mOtherUids);

      if (printUserGroupsForUsers) {
        item.emplace(GetKeyName(queryKeys::userGroups, useDescriptive), user.mGroups);
      }
      users.push_back(std::move(item));
    }

    root.emplace(GetKeyName(queryKeys::users, useDescriptive), std::move(users));
  }

  return Tree::FromJson(std::move(root));
}
} // namespace pep::structuredOutput