#pragma once

#include <optional>
#include <string>

namespace pep {

class UserRole {
private:
  bool mHasAssessorAccess = false;
  bool mCanCrossTabulate = false;

  UserRole() noexcept = default;

public:
  bool canSeeParticipantPersonalia() const noexcept { return mHasAssessorAccess; }
  bool canEditParticipantPersonalia() const noexcept { return mHasAssessorAccess; }
  bool canSetParticipantContext() const noexcept { return mHasAssessorAccess; }
  bool canManageDevices() const noexcept { return mHasAssessorAccess; }
  bool canRegisterParticipants() const noexcept { return mHasAssessorAccess; }
  bool canUpgradeStorage() const noexcept { return mHasAssessorAccess; }
  bool canPrintStickers() const noexcept { return mHasAssessorAccess; }
  bool canPrintSummary() const noexcept { return mHasAssessorAccess; }
  bool canCrossTabulate() const noexcept { return mCanCrossTabulate; }
  bool canEditVisitAdministeringAssessor() const noexcept { return mHasAssessorAccess; }

  static std::optional<UserRole> GetForOAuthRole(const std::string& oauthRole);
};

}
