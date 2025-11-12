#include <pep/auth/ServerTraits.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <pep/serialization/Error.hpp>

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>

namespace pep {

const std::string UserGroup::AccessManager = OnlyItemIn(ServerTraits::AccessManager().userGroups());
const std::unordered_set<std::string> UserGroup::Authserver = ServerTraits::AuthServer().userGroups();

void UserGroup::EnsureAccess(std::unordered_set<std::string> allowedUserGroups, std::string_view currentUserGroup, std::string_view actionDescription) {
  for (const auto &allowedGroup : allowedUserGroups) {
    if (currentUserGroup == allowedGroup) {
      return;
    }
  }
  throw Error((boost::format("%s is only allowed to the %s, you are currently %s") % actionDescription % boost::algorithm::join(allowedUserGroups, " and ") % currentUserGroup).str());
}

}
