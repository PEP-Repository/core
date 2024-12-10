#include <pep/auth/UserGroups.hpp>

#include <pep/serialization/Error.hpp>

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>

namespace pep::user_group {

void EnsureAccess(std::initializer_list<std::string> allowedUserGroups, std::string_view currentUserGroup, std::string_view actionDescription) {
  for (const auto &allowedGroup : allowedUserGroups) {
    if (currentUserGroup == allowedGroup) {
      return;
    }
  }
  throw Error((boost::format("%s is only allowed to the %s, you are currently %s") % actionDescription % boost::algorithm::join(allowedUserGroups, " and ") % currentUserGroup).str());
}

}
