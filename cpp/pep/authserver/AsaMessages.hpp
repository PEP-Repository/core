#pragma once

#include <pep/crypto/Signed.hpp>
#include <chrono>
#include <optional>
#include <vector>

namespace pep {

class AsaTokenRequest {
public:
  AsaTokenRequest() { }
  AsaTokenRequest(std::string subject, std::string group, Timestamp expirationTime)
    : mSubject(std::move(subject)), mGroup(std::move(group)), mExpirationTime(expirationTime) { }
  std::string mSubject;
  std::string mGroup;
  Timestamp mExpirationTime;
};

class AsaTokenResponse {
public:
  AsaTokenResponse() { }
  AsaTokenResponse(std::string token) : mToken(std::move(token)) { }
  std::string mToken;
};

class AsaCreateUser {
public:
  AsaCreateUser() { }
  explicit AsaCreateUser(std::string uid) : mUid(std::move(uid)) { };
  std::string mUid;
};

class AsaRemoveUser {
public:
  AsaRemoveUser() { }
  explicit  AsaRemoveUser(std::string uid) : mUid(std::move(uid)) { };
  std::string mUid;
};

class AsaAddUserIdentifier {
public:
  AsaAddUserIdentifier() { }
  AsaAddUserIdentifier(std::string existingUid, std::string newUid) : mExistingUid(std::move(existingUid)), mNewUid(std::move(newUid)) { }
  std::string mExistingUid;
  std::string mNewUid;
};

class AsaRemoveUserIdentifier {
public:
  AsaRemoveUserIdentifier() { }
  AsaRemoveUserIdentifier(std::string uid) : mUid(std::move(uid)) { }
  std::string mUid;
};

class UserGroupProperties {
public:
  UserGroupProperties() { }
  explicit UserGroupProperties(std::optional<std::chrono::seconds> maxAuthValidity) : mMaxAuthValidity(maxAuthValidity) { }
  std::optional<std::chrono::seconds> mMaxAuthValidity;
};

class AsaCreateUserGroup {
public:
  AsaCreateUserGroup() { }
  AsaCreateUserGroup(std::string name, UserGroupProperties properties) : mName(std::move(name)), mProperties(properties) { }
  std::string mName;
  UserGroupProperties mProperties;
};

class AsaModifyUserGroup {
public:
  AsaModifyUserGroup() { }
  AsaModifyUserGroup(std::string name, UserGroupProperties properties) : mName(std::move(name)), mProperties(properties) { }
  std::string mName;
  UserGroupProperties mProperties;
};

class AsaRemoveUserGroup {
public:
  AsaRemoveUserGroup() { }
  AsaRemoveUserGroup(std::string name) : mName(std::move(name)) { }
  std::string mName;
};

class AsaAddUserToGroup {
public:
  AsaAddUserToGroup() { }
  AsaAddUserToGroup(std::string uid, std::string group)
    : mUid(std::move(uid)), mGroup(std::move(group)) { }
  std::string mUid;
  std::string mGroup;
};

class AsaRemoveUserFromGroup {
public:
  AsaRemoveUserFromGroup() { }
  AsaRemoveUserFromGroup(std::string uid, std::string group) : mUid(std::move(uid)), mGroup(std::move(group)) { }
  std::string mUid;
  std::string mGroup;
};

class AsaMutationRequest {
public:
  std::vector<AsaCreateUser> mCreateUser;
  std::vector<AsaRemoveUser> mRemoveUser;

  std::vector<AsaAddUserIdentifier> mAddUserIdentifier;
  std::vector<AsaRemoveUserIdentifier> mRemoveUserIdentifier;

  std::vector<AsaCreateUserGroup> mCreateUserGroup;
  std::vector<AsaRemoveUserGroup> mRemoveUserGroup;
  std::vector<AsaModifyUserGroup> mModifyUserGroup;

  std::vector<AsaAddUserToGroup> mAddUserToGroup;
  std::vector<AsaRemoveUserFromGroup> mRemoveUserFromGroup;
};

class AsaMutationResponse {
};

class AsaQuery {
public:
  Timestamp mAt;
  std::string mGroupFilter;
  std::string mUserFilter;
};

class AsaQRUserGroup {
public:
  AsaQRUserGroup() { }
  AsaQRUserGroup(std::string name, UserGroupProperties properties)
    : mName(std::move(name)), mProperties(properties) { }

  std::string mName;
  UserGroupProperties mProperties;
};

class AsaQRUser {
public:
  AsaQRUser() { }
  AsaQRUser(std::vector<std::string> uids, std::vector<std::string> groups)
    : mUids(std::move(uids)), mGroups(std::move(groups)) { }

  std::vector<std::string> mUids;
  std::vector<std::string> mGroups;
};

class AsaQueryResponse {
public:
  std::vector<AsaQRUser> mUsers;
  std::vector<AsaQRUserGroup> mUserGroups;
};

using SignedAsaTokenRequest = Signed<AsaTokenRequest>;
using SignedAsaMutationRequest = Signed<AsaMutationRequest>;
using SignedAsaQuery = Signed<AsaQuery>;

}
