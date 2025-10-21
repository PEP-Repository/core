#pragma once

#include <pep/crypto/Signed.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <vector>

namespace pep {

class CreateUser {
public:
  CreateUser() = default;
  explicit CreateUser(std::string uid) : mUid(std::move(uid)) { };
  std::string mUid;
};

class RemoveUser {
public:
  RemoveUser() = default;
  explicit  RemoveUser(std::string uid) : mUid(std::move(uid)) { };
  std::string mUid;
};

class AddUserIdentifier {
public:
  AddUserIdentifier() = default;
  AddUserIdentifier(std::string existingUid, std::string newUid, bool isPrimaryId, bool isDisplayId)
    : mExistingUid(std::move(existingUid)), mNewUid(std::move(newUid)), mIsPrimaryId(isPrimaryId),mIsDisplayId(isDisplayId) { }
  std::string mExistingUid;
  std::string mNewUid;
  bool mIsPrimaryId;
  bool mIsDisplayId;
};

class RemoveUserIdentifier {
public:
  RemoveUserIdentifier() = default;
  RemoveUserIdentifier(std::string uid) : mUid(std::move(uid)) { }
  std::string mUid;
};

class CreateUserGroup {
public:
  CreateUserGroup() = default;
  CreateUserGroup(UserGroup userGroup) : mUserGroup(std::move(userGroup)) { }
  UserGroup mUserGroup;
};

class ModifyUserGroup {
public:
  ModifyUserGroup() = default;
  ModifyUserGroup(UserGroup userGroup) : mUserGroup(std::move(userGroup)) { }
  UserGroup mUserGroup;
};

class RemoveUserGroup {
public:
  RemoveUserGroup() = default;
  RemoveUserGroup(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AddUserToGroup {
public:
  AddUserToGroup() = default;
  AddUserToGroup(std::string uid, std::string group)
    : mUid(std::move(uid)), mGroup(std::move(group)) { }
  std::string mUid;
  std::string mGroup;
};

class RemoveUserFromGroup {
public:
  RemoveUserFromGroup() = default;
  RemoveUserFromGroup(std::string uid, std::string group, bool blockTokens) : mUid(std::move(uid)), mGroup(std::move(group)), mBlockTokens(blockTokens) { }
  std::string mUid;
  std::string mGroup;
  bool mBlockTokens = false;
};

class UserMutationRequest {
public:
  std::vector<CreateUser> mCreateUser;
  std::vector<RemoveUser> mRemoveUser;

  std::vector<AddUserIdentifier> mAddUserIdentifier;
  std::vector<RemoveUserIdentifier> mRemoveUserIdentifier;
  std::vector<std::string> mSetPrimaryId;
  std::vector<std::string> mUnsetPrimaryId;
  std::vector<std::string> mSetDisplayId;

  std::vector<CreateUserGroup> mCreateUserGroup;
  std::vector<RemoveUserGroup> mRemoveUserGroup;
  std::vector<ModifyUserGroup> mModifyUserGroup;

  std::vector<AddUserToGroup> mAddUserToGroup;
  std::vector<RemoveUserFromGroup> mRemoveUserFromGroup;
};

class UserMutationResponse {
};

class UserQuery {
public:
  Timestamp mAt;
  std::string mGroupFilter;
  std::string mUserFilter;
};

class QRUser {
public:
  QRUser() = default;
  QRUser(std::optional<std::string> displayId, std::optional<std::string>  primaryId, std::vector<std::string> otherUids, std::vector<std::string> groups)
    : mDisplayId(std::move(displayId)), mPrimaryId(std::move(primaryId)), mOtherUids(std::move(otherUids)), mGroups(std::move(groups)) { }

  std::optional<std::string> mDisplayId;
  std::optional<std::string> mPrimaryId;
  std::vector<std::string> mOtherUids;
  std::vector<std::string> mGroups;

  [[nodiscard]] auto operator<=>(const QRUser&) const = default;

  friend std::ostream& operator<<(std::ostream& out, const QRUser& user) {
    out << user.mDisplayId.value_or("[NO DISPLAY ID]") << ":{";
    out << "uids:{";
    if (user.mPrimaryId) {
      out << "*" << *user.mPrimaryId;
    }
    for (bool first = !user.mPrimaryId; const auto& uid : user.mOtherUids) {
      if (!std::exchange(first, false)) { out << ", "; }
      out << uid;
    }
    out << '}';

    out << " groups:{";
    for (bool first = true; const auto& group : user.mGroups) {
      if (!std::exchange(first, false)) { out << ", "; }
      out << group;
    }
    out << '}';
    out << '}';
    return out;
  }
};

class UserQueryResponse {
public:
  std::vector<QRUser> mUsers;
  std::vector<UserGroup> mUserGroups;
};

using SignedUserMutationRequest = Signed<UserMutationRequest>;
using SignedUserQuery = Signed<UserQuery>;

}
