#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <vector>

namespace pep {

class CreateUser {
public:
  CreateUser() = default;
  explicit CreateUser(std::string uid) : uid_(std::move(uid)) { };
  std::string uid_;
};

class RemoveUser {
public:
  RemoveUser() = default;
  explicit  RemoveUser(std::string uid) : uid_(std::move(uid)) { };
  std::string uid_;
};

class AddUserIdentifier {
public:
  AddUserIdentifier() = default;
  AddUserIdentifier(std::string existingUid, std::string newUid, bool isPrimaryId, bool isDisplayId)
    : existingUid_(std::move(existingUid)), newUid_(std::move(newUid)), isPrimaryId_(isPrimaryId),isDisplayId_(isDisplayId) { }
  std::string existingUid_;
  std::string newUid_;
  bool isPrimaryId_ = false;
  bool isDisplayId_ = false;
};

class RemoveUserIdentifier {
public:
  RemoveUserIdentifier() = default;
  RemoveUserIdentifier(std::string uid) : uid_(std::move(uid)) { }
  std::string uid_;
};

class CreateUserGroup {
public:
  CreateUserGroup() = default;
  CreateUserGroup(UserGroup userGroup) : userGroup_(std::move(userGroup)) { }
  UserGroup userGroup_;
};

class ModifyUserGroup {
public:
  ModifyUserGroup() = default;
  ModifyUserGroup(UserGroup userGroup) : userGroup_(std::move(userGroup)) { }
  UserGroup userGroup_;
};

class RemoveUserGroup {
public:
  RemoveUserGroup() = default;
  RemoveUserGroup(std::string name) : name_(std::move(name)) { }
  std::string name_;
};

class AddUserToGroup {
public:
  AddUserToGroup() = default;
  AddUserToGroup(std::string uid, std::string group)
    : uid_(std::move(uid)), group_(std::move(group)) { }
  std::string uid_;
  std::string group_;
};

class RemoveUserFromGroup {
public:
  RemoveUserFromGroup() = default;
  RemoveUserFromGroup(std::string uid, std::string group, bool blockTokens) : uid_(std::move(uid)), group_(std::move(group)), blockTokens_(blockTokens) { }
  std::string uid_;
  std::string group_;
  bool blockTokens_ = false;
};

class UserMutationRequest {
public:
  std::vector<CreateUser> createUser_;
  std::vector<RemoveUser> removeUser_;

  std::vector<AddUserIdentifier> addUserIdentifier_;
  std::vector<RemoveUserIdentifier> removeUserIdentifier_;
  std::vector<std::string> setPrimaryId_;
  std::vector<std::string> unsetPrimaryId_;
  std::vector<std::string> setDisplayId_;

  std::vector<CreateUserGroup> createUserGroup_;
  std::vector<RemoveUserGroup> removeUserGroup_;
  std::vector<ModifyUserGroup> modifyUserGroup_;

  std::vector<AddUserToGroup> addUserToGroup_;
  std::vector<RemoveUserFromGroup> removeUserFromGroup_;
};

class UserMutationResponse {
};

class UserQuery {
public:
  // Use nullopt for current server time, such that a wrong client time does not influence query
  std::optional<Timestamp> at_;
  std::string groupFilter_;
  std::string userFilter_;
};

class QRUser {
public:
  QRUser() = default;
  QRUser(std::optional<std::string> displayId, std::optional<std::string>  primaryId, std::vector<std::string> otherUids, std::vector<std::string> groups)
    : displayId_(std::move(displayId)), primaryId_(std::move(primaryId)), otherUids_(std::move(otherUids)), groups_(std::move(groups)) { }

  std::optional<std::string> displayId_;
  std::optional<std::string> primaryId_;
  std::vector<std::string> otherUids_;
  std::vector<std::string> groups_;

  [[nodiscard]] auto operator<=>(const QRUser&) const = default;

  friend std::ostream& operator<<(std::ostream& out, const QRUser& user) {
    out << user.displayId_.value_or("[NO DISPLAY ID]") << ":{";
    out << "uids:{";
    if (user.primaryId_) {
      out << "*" << *user.primaryId_;
    }
    for (bool first = !user.primaryId_; const auto& uid : user.otherUids_) {
      if (!std::exchange(first, false)) { out << ", "; }
      out << uid;
    }
    out << '}';

    out << " groups:{";
    for (bool first = true; const auto& group : user.groups_) {
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
  std::vector<QRUser> users_;
  std::vector<UserGroup> userGroups_;
};

using SignedUserMutationRequest = Signed<UserMutationRequest>;
using SignedUserQuery = Signed<UserQuery>;

}
