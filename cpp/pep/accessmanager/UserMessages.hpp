#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <vector>

namespace pep {

struct CreateUser {
  std::string uid;
};

struct RemoveUser {
  std::string uid;
};

struct AddUserIdentifier {
  std::string existingUid;
  std::string newUid;
  bool isPrimaryId = false;
  bool isDisplayId = false;
};

struct RemoveUserIdentifier {
  std::string uid;
};

struct CreateUserGroup {
  UserGroup userGroup;
};

struct ModifyUserGroup {
  UserGroup userGroup;
};

struct RemoveUserGroup {
  std::string name;
};

struct AddUserToGroup {
  std::string uid;
  std::string group;
};

struct RemoveUserFromGroup {
public:
  std::string uid;
  std::string group;
  bool blockTokens = false;
};

struct UserMutationRequest {
  std::vector<CreateUser> createUser;
  std::vector<RemoveUser> removeUser;

  std::vector<AddUserIdentifier> addUserIdentifier;
  std::vector<RemoveUserIdentifier> removeUserIdentifier;
  std::vector<std::string> setPrimaryId;
  std::vector<std::string> unsetPrimaryId;
  std::vector<std::string> setDisplayId;

  std::vector<CreateUserGroup> createUserGroup;
  std::vector<RemoveUserGroup> removeUserGroup;
  std::vector<ModifyUserGroup> modifyUserGroup;

  std::vector<AddUserToGroup> addUserToGroup;
  std::vector<RemoveUserFromGroup> removeUserFromGroup;
};

struct UserMutationResponse {};

struct UserQuery {
  // Use nullopt for current server time, such that a wrong client time does not influence query
  std::optional<Timestamp> at;
  std::string groupFilter;
  std::string userFilter;
};

struct QRUser {
  std::optional<std::string> displayId;
  std::optional<std::string> primaryId;
  std::vector<std::string> otherUids;
  std::vector<std::string> groups;

  [[nodiscard]] auto operator<=>(const QRUser&) const = default;

  friend std::ostream& operator<<(std::ostream& out, const QRUser& user) {
    out << user.displayId.value_or("[NO DISPLAY ID]") << ":{";
    out << "uids:{";
    if (user.primaryId) {
      out << "*" << *user.primaryId;
    }
    for (bool first = !user.primaryId; const auto& uid : user.otherUids) {
      if (!std::exchange(first, false)) { out << ", "; }
      out << uid;
    }
    out << '}';

    out << " groups:{";
    for (bool first = true; const auto& group : user.groups) {
      if (!std::exchange(first, false)) { out << ", "; }
      out << group;
    }
    out << '}';
    out << '}';
    return out;
  }
};

struct UserQueryResponse {
  std::vector<QRUser> users;
  std::vector<UserGroup> userGroups;
};

using SignedUserMutationRequest = Signed<UserMutationRequest>;
using SignedUserQuery = Signed<UserQuery>;

}
