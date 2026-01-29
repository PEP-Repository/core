#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <chrono>
#include <unordered_set>

namespace pep {

class UserGroup {
public:
  UserGroup() = default;
  UserGroup(std::string name, std::optional<std::chrono::seconds> maxAuthValidity)
    : mName(std::move(name)), mMaxAuthValidity(maxAuthValidity) { }


  /*!
   * \brief Check access, throws if access is denied
   * \throws pep::Error if access is denied
   */
  static void EnsureAccess(std::unordered_set<std::string> allowedUserGroups, std::string_view currentUserGroup, std::string_view
                           actionDescription = "This action");

  [[nodiscard]] auto operator<=>(const UserGroup&) const = default;

  friend std::ostream& operator<<(std::ostream& out, const UserGroup& group) {
    out << '{';
    if (group.mMaxAuthValidity) { out << " maxAuthValidity:" << *group.mMaxAuthValidity; }
    out << '}';
    return out;
  }

  std::string mName;
  std::optional<std::chrono::seconds> mMaxAuthValidity;

  // Special access groups that are checked in the code
  inline static const std::string AccessAdministrator{"Access Administrator"},
    DataAdministrator{"Data Administrator"},
    SystemAdministrator{"System Administrator"},
    ResearchAssessor{"Research Assessor"},
    Watchdog{"Watchdog"},
    Monitor{"Monitor"};

  static const std::string AccessManager;
  static const std::unordered_set<std::string> Authserver;
};

}
