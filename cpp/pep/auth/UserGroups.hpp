#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace pep::user_group {

// Special access groups that are checked in the code
inline const std::string
  AccessAdministrator{"Access Administrator"},
  DataAdministrator{"Data Administrator"},
  ResearchAssessor{"Research Assessor"},
  Watchdog{"Watchdog"},
  Monitor{"Monitor"};

/*!
 * \brief Check access, throws if access is denied
 * \throws pep::Error if access is denied
 */
void EnsureAccess(std::initializer_list<std::string> allowedUserGroups, std::string_view currentUserGroup, std::string_view actionDescription = "This action");

}
