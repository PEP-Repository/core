#include <pep/assessor/UserRole.hpp>

#include <pep/auth/UserGroups.hpp>

namespace pep {

std::optional<UserRole> UserRole::GetForOAuthRole(const std::string& oauthRole) {
  UserRole role;
  if (oauthRole == user_group::DataAdministrator) {
    role.mCanCrossTabulate = true;
  } else if (oauthRole == user_group::ResearchAssessor) {
    role.mHasAssessorAccess = true;
  } else if (oauthRole == user_group::Monitor) {
    // no special permissions for Research Monitor role
  } else {
    // return nullopt for all other roles
    return std::nullopt;
  }

  return role;
}

}
