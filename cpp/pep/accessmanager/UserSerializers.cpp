#include <pep/accessmanager/UserSerializers.hpp>
#include <pep/auth/UserGroupSerializers.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

CreateUser Serializer<CreateUser>::fromProtocolBuffer(proto::CreateUser&& source) const {
  return CreateUser(std::move(*source.mutable_uid()));
}

void Serializer<CreateUser>::moveIntoProtocolBuffer(proto::CreateUser& dest, CreateUser value) const {
  *dest.mutable_uid() = std::move(value.uid_);
}

RemoveUser Serializer<RemoveUser>::fromProtocolBuffer(proto::RemoveUser&& source) const {
  return RemoveUser(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUser>::moveIntoProtocolBuffer(proto::RemoveUser& dest, RemoveUser value) const {
  *dest.mutable_uid() = std::move(value.uid_);
}

AddUserIdentifier Serializer<AddUserIdentifier>::fromProtocolBuffer(proto::AddUserIdentifier&& source) const {
  return AddUserIdentifier(std::move(*source.mutable_existinguid()), std::move(*source.mutable_newuid()), source.is_primary_id(), source.is_display_id());
}

void Serializer<AddUserIdentifier>::moveIntoProtocolBuffer(proto::AddUserIdentifier& dest, AddUserIdentifier value) const {
  *dest.mutable_existinguid() = std::move(value.existingUid_);
  *dest.mutable_newuid() = std::move(value.newUid_);
  dest.set_is_primary_id(value.isPrimaryId_);
  dest.set_is_display_id(value.isDisplayId_);
}

RemoveUserIdentifier Serializer<RemoveUserIdentifier>::fromProtocolBuffer(proto::RemoveUserIdentifier&& source) const {
  return RemoveUserIdentifier(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUserIdentifier>::moveIntoProtocolBuffer(proto::RemoveUserIdentifier& dest, RemoveUserIdentifier value) const {
  *dest.mutable_uid() = std::move(value.uid_);
}

CreateUserGroup Serializer<CreateUserGroup>::fromProtocolBuffer(proto::CreateUserGroup&& source) const {
  return CreateUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<CreateUserGroup>::moveIntoProtocolBuffer(proto::CreateUserGroup& dest, CreateUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.userGroup_));
}

RemoveUserGroup Serializer<RemoveUserGroup>::fromProtocolBuffer(proto::RemoveUserGroup&& source) const {
  return RemoveUserGroup(std::move(*source.mutable_name()));
}

void Serializer<RemoveUserGroup>::moveIntoProtocolBuffer(proto::RemoveUserGroup& dest, RemoveUserGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
}

ModifyUserGroup Serializer<ModifyUserGroup>::fromProtocolBuffer(proto::ModifyUserGroup&& source) const {
  return ModifyUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<ModifyUserGroup>::moveIntoProtocolBuffer(proto::ModifyUserGroup& dest, ModifyUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.userGroup_));
}

AddUserToGroup Serializer<AddUserToGroup>::fromProtocolBuffer(proto::AddUserToGroup&& source) const {
  std::optional<Timestamp> expiration;
  if (source.has_expiration()) {
    expiration = Serialization::FromProtocolBuffer(std::move(*source.mutable_expiration()));
  }
  return AddUserToGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group()),
    expiration
  );
}

void Serializer<AddUserToGroup>::moveIntoProtocolBuffer(proto::AddUserToGroup& dest, AddUserToGroup value) const {
  *dest.mutable_uid() = std::move(value.uid_);
  *dest.mutable_group() = std::move(value.group_);
  if (value.expiration_) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_expiration(), *value.expiration_);
  }
}

RemoveUserFromGroup Serializer<RemoveUserFromGroup>::fromProtocolBuffer(proto::RemoveUserFromGroup&& source) const {
  return RemoveUserFromGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group()),
    source.block_tokens()
  );
}

void Serializer<RemoveUserFromGroup>::moveIntoProtocolBuffer(proto::RemoveUserFromGroup& dest, RemoveUserFromGroup value) const {
  *dest.mutable_uid() = std::move(value.uid_);
  *dest.mutable_group() = std::move(value.group_);
  dest.set_block_tokens(value.blockTokens_);
}

UpdateExpiration Serializer<UpdateExpiration>::fromProtocolBuffer(proto::UpdateExpiration&& source) const {
  std::optional<Timestamp> expiration;
  if (source.has_expiration()) {
    expiration = Serialization::FromProtocolBuffer(std::move(*source.mutable_expiration()));
  }
  return UpdateExpiration(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group()),
    expiration
  );
}

void Serializer<UpdateExpiration>::moveIntoProtocolBuffer(proto::UpdateExpiration& dest, UpdateExpiration value) const {
  *dest.mutable_uid() = std::move(value.uid_);
  *dest.mutable_group() = std::move(value.group_);
  if (value.expiration_) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_expiration(), *value.expiration_);
  }
}

UserMutationRequest Serializer<UserMutationRequest>::fromProtocolBuffer(proto::UserMutationRequest&& source) const {
  UserMutationRequest result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.createUser_,
    std::move(*source.mutable_create_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUser_,
    std::move(*source.mutable_remove_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addUserIdentifier_,
    std::move(*source.mutable_add_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserIdentifier_,
    std::move(*source.mutable_remove_user_identifier()));
  result.setPrimaryId_ = RangeToVector(MoveElements(*source.mutable_set_primary_identifier()));
  result.unsetPrimaryId_ = RangeToVector(MoveElements(*source.mutable_unset_primary_identifier()));
  result.setDisplayId_ = RangeToVector(MoveElements(*source.mutable_set_display_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createUserGroup_,
    std::move(*source.mutable_create_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserGroup_,
    std::move(*source.mutable_remove_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.modifyUserGroup_,
    std::move(*source.mutable_modify_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addUserToGroup_,
    std::move(*source.mutable_add_user_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserFromGroup_,
    std::move(*source.mutable_remove_user_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.updateExpiration_,
    std::move(*source.mutable_update_expiration()));
  return result;
}

void Serializer<UserMutationRequest>::moveIntoProtocolBuffer(proto::UserMutationRequest& dest, UserMutationRequest value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user(),
    std::move(value.createUser_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user(),
    std::move(value.removeUser_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_identifier(),
    std::move(value.addUserIdentifier_));
  auto moveSetPrimaryId = MoveElements(value.setPrimaryId_);
  dest.mutable_set_primary_identifier()->Assign(moveSetPrimaryId.begin(), moveSetPrimaryId.end());
  auto moveUnsetPrimaryId = MoveElements(value.unsetPrimaryId_);
  dest.mutable_unset_primary_identifier()->Assign(moveUnsetPrimaryId.begin(), moveUnsetPrimaryId.end());
  auto moveSetDisplayId = MoveElements(value.setDisplayId_);
  dest.mutable_set_display_identifier()->Assign(moveSetDisplayId.begin(), moveSetDisplayId.end());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_identifier(),
    std::move(value.removeUserIdentifier_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user_group(),
    std::move(value.createUserGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_group(),
    std::move(value.removeUserGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_modify_user_group(),
    std::move(value.modifyUserGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_to_group(),
    std::move(value.addUserToGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_from_group(),
    std::move(value.removeUserFromGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_update_expiration(),
    std::move(value.updateExpiration_));
}

UserQueryResponse Serializer<UserQueryResponse>::fromProtocolBuffer(proto::UserQueryResponse&& source) const {
  UserQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mUsers,
    std::move(*source.mutable_users()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mUserGroups,
    std::move(*source.mutable_user_groups()));
  return result;
}

void Serializer<UserQueryResponse>::moveIntoProtocolBuffer(proto::UserQueryResponse& dest, UserQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_users(),
    std::move(value.mUsers));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_user_groups(),
    std::move(value.mUserGroups));
}

UserQuery Serializer<UserQuery>::fromProtocolBuffer(proto::UserQuery&& source) const {
  return {
    .mAt = source.has_at()
        ? std::optional(Serialization::FromProtocolBuffer(std::move(*source.mutable_at())))
        : std::nullopt,
    .mGroupFilter = std::move(*source.mutable_group_filter()),
    .mUserFilter = std::move(*source.mutable_user_filter()),
  };
}

void Serializer<UserQuery>::moveIntoProtocolBuffer(proto::UserQuery& dest, UserQuery value) const {
  if (value.mAt) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), *value.mAt);
  }
  *dest.mutable_group_filter() = std::move(value.mGroupFilter);
  *dest.mutable_user_filter() = std::move(value.mUserFilter);
}

QRUserGroupMembership Serializer<QRUserGroupMembership>::fromProtocolBuffer(proto::QRUserGroupMembership&& source) const {
  std::optional<Timestamp> expiration;
  if (source.has_expiration()) {
    expiration = Serialization::FromProtocolBuffer(std::move(*source.mutable_expiration()));
  }
  return QRUserGroupMembership{std::move(*source.mutable_user_group()), expiration};
}

void Serializer<QRUserGroupMembership>::moveIntoProtocolBuffer(proto::QRUserGroupMembership& dest, QRUserGroupMembership value) const {
  *dest.mutable_user_group() = std::move(value.userGroup);
  if (value.expiration) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_expiration(), *value.expiration);
  }
}

QRUser Serializer<QRUser>::fromProtocolBuffer(proto::QRUser&& source) const {
  std::optional<std::string> primaryId;
  if (source.has_primary_id()) {
    primaryId = std::move(*source.mutable_primary_id());
  }
  std::optional<std::string> displayId;
  if (source.has_display_id()) {
    displayId = std::move(*source.mutable_display_id());
  }
  std::vector<std::string> otherUids = RangeToVector(MoveElements(*source.mutable_other_uids()));
  std::vector<QRUserGroupMembership> groups;
  Serialization::AssignFromRepeatedProtocolBuffer(groups,
    std::move(*source.mutable_groups()));

  return QRUser(std::move(displayId), std::move(primaryId), std::move(otherUids), std::move(groups));
}

void Serializer<QRUser>::moveIntoProtocolBuffer(proto::QRUser& dest, QRUser value) const {
  if (value.mDisplayId)
    *dest.mutable_display_id() = std::move(*value.mDisplayId);
  if (value.mPrimaryId)
    *dest.mutable_primary_id() = std::move(*value.mPrimaryId);
  auto moveOtherUids = MoveElements(value.mOtherUids);
  dest.mutable_other_uids()->Assign(moveOtherUids.begin(), moveOtherUids.end());

  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_groups(),
    std::move(value.mGroups));

}

}
