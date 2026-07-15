#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <chrono>
#include <unordered_set>

namespace pep {

struct UserGroup {
  /*!
   * \brief Check access, throws if access is denied
   * \throws pep::Error if access is denied
   */
  static void EnsureAccess(std::unordered_set<std::string> allowedUserGroups, std::string_view currentUserGroup, std::string_view
                           actionDescription = "This action");

  [[nodiscard]] auto operator<=>(const UserGroup&) const = default;

  friend std::ostream& operator<<(std::ostream& out, const UserGroup& group) {
    out << '{';
    if (group.maxAuthValidity) { out << " maxAuthValidity:" << *group.maxAuthValidity; }
    out << '}';
    return out;
  }

  std::string name;
  std::optional<std::chrono::seconds> maxAuthValidity;

  // Special access groups that are checked in the code
  inline static const std::string
    AccessAdministrator { "Access Administrator" },
    DataAdministrator   { "Data Administrator" },
    Monitor             { "Monitor" },
    ResearchAssessor    { "Research Assessor" },
    SystemAdministrator { "System Administrator" },
    Watchdog            { "Watchdog" };

  static const std::string AccessManager;
  static const std::unordered_set<std::string> Authserver;
};

}
