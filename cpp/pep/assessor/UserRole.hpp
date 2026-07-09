#pragma once

#include <optional>
#include <string>

namespace pep {

class UserRole {
private:
  bool hasAssessorAccess_ = false;
  bool canCrossTabulate_ = false;

  UserRole() noexcept = default;

public:
  bool canSeeParticipantPersonalia() const noexcept { return hasAssessorAccess_; }
  bool canEditParticipantPersonalia() const noexcept { return hasAssessorAccess_; }
  bool canSetParticipantContext() const noexcept { return hasAssessorAccess_; }
  bool canManageDevices() const noexcept { return hasAssessorAccess_; }
  bool canRegisterParticipants() const noexcept { return hasAssessorAccess_; }
  bool canUpgradeStorage() const noexcept { return hasAssessorAccess_; }
  bool canPrintStickers() const noexcept { return hasAssessorAccess_; }
  bool canPrintSummary() const noexcept { return hasAssessorAccess_; }
  bool canCrossTabulate() const noexcept { return canCrossTabulate_; }
  bool canEditVisitAdministeringAssessor() const noexcept { return hasAssessorAccess_; }

  static std::optional<UserRole> GetForOAuthRole(const std::string& oauthRole);
};

}
