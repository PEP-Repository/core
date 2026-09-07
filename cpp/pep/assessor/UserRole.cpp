#include <pep/assessor/UserRole.hpp>

#include <pep/auth/UserGroup.hpp>

namespace pep {

std::optional<UserRole> UserRole::GetForOAuthRole(const std::string& oauthRole) {
  UserRole role;
  if (oauthRole == UserGroup::DataAdministrator) {
    role.canCrossTabulate_ = true;
  } else if (oauthRole == UserGroup::ResearchAssessor) {
    role.hasAssessorAccess_ = true;
  } else if (oauthRole == UserGroup::Monitor) {
    // no special permissions for Research Monitor role
  } else {
    // return nullopt for all other roles
    return std::nullopt;
  }

  return role;
}

}
