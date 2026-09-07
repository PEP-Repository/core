#include <pep/accessmanager/UserSerializers.hpp>
#include <pep/auth/UserGroupSerializers.hpp>
#include <pep/serialization/TimestampSerializer.hpp>

namespace pep {

CreateUser Serializer<CreateUser>::fromProtocolBuffer(proto::CreateUser&& source) const {
  return CreateUser(std::move(*source.mutable_uid()));
}

void Serializer<CreateUser>::moveIntoProtocolBuffer(proto::CreateUser& dest, CreateUser value) const {
  *dest.mutable_uid() = std::move(value.uid);
}

RemoveUser Serializer<RemoveUser>::fromProtocolBuffer(proto::RemoveUser&& source) const {
  return RemoveUser(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUser>::moveIntoProtocolBuffer(proto::RemoveUser& dest, RemoveUser value) const {
  *dest.mutable_uid() = std::move(value.uid);
}

AddUserIdentifier Serializer<AddUserIdentifier>::fromProtocolBuffer(proto::AddUserIdentifier&& source) const {
  return AddUserIdentifier(std::move(*source.mutable_existinguid()), std::move(*source.mutable_newuid()), source.is_primary_id(), source.is_display_id());
}

void Serializer<AddUserIdentifier>::moveIntoProtocolBuffer(proto::AddUserIdentifier& dest, AddUserIdentifier value) const {
  *dest.mutable_existinguid() = std::move(value.existingUid);
  *dest.mutable_newuid() = std::move(value.newUid);
  dest.set_is_primary_id(value.isPrimaryId);
  dest.set_is_display_id(value.isDisplayId);
}

RemoveUserIdentifier Serializer<RemoveUserIdentifier>::fromProtocolBuffer(proto::RemoveUserIdentifier&& source) const {
  return RemoveUserIdentifier(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUserIdentifier>::moveIntoProtocolBuffer(proto::RemoveUserIdentifier& dest, RemoveUserIdentifier value) const {
  *dest.mutable_uid() = std::move(value.uid);
}

CreateUserGroup Serializer<CreateUserGroup>::fromProtocolBuffer(proto::CreateUserGroup&& source) const {
  return CreateUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<CreateUserGroup>::moveIntoProtocolBuffer(proto::CreateUserGroup& dest, CreateUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.userGroup));
}

RemoveUserGroup Serializer<RemoveUserGroup>::fromProtocolBuffer(proto::RemoveUserGroup&& source) const {
  return RemoveUserGroup(std::move(*source.mutable_name()));
}

void Serializer<RemoveUserGroup>::moveIntoProtocolBuffer(proto::RemoveUserGroup& dest, RemoveUserGroup value) const {
  *dest.mutable_name() = std::move(value.name);
}

ModifyUserGroup Serializer<ModifyUserGroup>::fromProtocolBuffer(proto::ModifyUserGroup&& source) const {
  return ModifyUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<ModifyUserGroup>::moveIntoProtocolBuffer(proto::ModifyUserGroup& dest, ModifyUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.userGroup));
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
  *dest.mutable_uid() = std::move(value.uid);
  *dest.mutable_group() = std::move(value.group);
  if (value.expiration) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_expiration(), *value.expiration);
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
  *dest.mutable_uid() = std::move(value.uid);
  *dest.mutable_group() = std::move(value.group);
  dest.set_block_tokens(value.blockTokens);
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
  *dest.mutable_uid() = std::move(value.uid);
  *dest.mutable_group() = std::move(value.group);
  if (value.expiration) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_expiration(), *value.expiration);
  }
}

UserMutationRequest Serializer<UserMutationRequest>::fromProtocolBuffer(proto::UserMutationRequest&& source) const {
  UserMutationRequest result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.createUser,
    std::move(*source.mutable_create_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUser,
    std::move(*source.mutable_remove_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addUserIdentifier,
    std::move(*source.mutable_add_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserIdentifier,
    std::move(*source.mutable_remove_user_identifier()));
  result.setPrimaryId = RangeToVector(MoveElements(*source.mutable_set_primary_identifier()));
  result.unsetPrimaryId = RangeToVector(MoveElements(*source.mutable_unset_primary_identifier()));
  result.setDisplayId = RangeToVector(MoveElements(*source.mutable_set_display_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createUserGroup,
    std::move(*source.mutable_create_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserGroup,
    std::move(*source.mutable_remove_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.modifyUserGroup,
    std::move(*source.mutable_modify_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addUserToGroup,
    std::move(*source.mutable_add_user_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeUserFromGroup,
    std::move(*source.mutable_remove_user_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.updateExpiration,
    std::move(*source.mutable_update_expiration()));
  return result;
}

void Serializer<UserMutationRequest>::moveIntoProtocolBuffer(proto::UserMutationRequest& dest, UserMutationRequest value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user(),
    std::move(value.createUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user(),
    std::move(value.removeUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_identifier(),
    std::move(value.addUserIdentifier));
  auto moveSetPrimaryId = MoveElements(value.setPrimaryId);
  dest.mutable_set_primary_identifier()->Assign(moveSetPrimaryId.begin(), moveSetPrimaryId.end());
  auto moveUnsetPrimaryId = MoveElements(value.unsetPrimaryId);
  dest.mutable_unset_primary_identifier()->Assign(moveUnsetPrimaryId.begin(), moveUnsetPrimaryId.end());
  auto moveSetDisplayId = MoveElements(value.setDisplayId);
  dest.mutable_set_display_identifier()->Assign(moveSetDisplayId.begin(), moveSetDisplayId.end());
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_identifier(),
    std::move(value.removeUserIdentifier));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user_group(),
    std::move(value.createUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_group(),
    std::move(value.removeUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_modify_user_group(),
    std::move(value.modifyUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_to_group(),
    std::move(value.addUserToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_from_group(),
    std::move(value.removeUserFromGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_update_expiration(),
    std::move(value.updateExpiration));
}

UserQueryResponse Serializer<UserQueryResponse>::fromProtocolBuffer(proto::UserQueryResponse&& source) const {
  UserQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.users,
    std::move(*source.mutable_users()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.userGroups,
    std::move(*source.mutable_user_groups()));
  return result;
}

void Serializer<UserQueryResponse>::moveIntoProtocolBuffer(proto::UserQueryResponse& dest, UserQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_users(),
    std::move(value.users));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_user_groups(),
    std::move(value.userGroups));
}

UserQuery Serializer<UserQuery>::fromProtocolBuffer(proto::UserQuery&& source) const {
  return {
    .at = source.has_at()
        ? std::optional(Serialization::FromProtocolBuffer(std::move(*source.mutable_at())))
        : std::nullopt,
    .groupFilter = std::move(*source.mutable_group_filter()),
    .userFilter = std::move(*source.mutable_user_filter()),
  };
}

void Serializer<UserQuery>::moveIntoProtocolBuffer(proto::UserQuery& dest, UserQuery value) const {
  if (value.at) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), *value.at);
  }
  *dest.mutable_group_filter() = std::move(value.groupFilter);
  *dest.mutable_user_filter() = std::move(value.userFilter);
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
  if (value.displayId)
    *dest.mutable_display_id() = std::move(*value.displayId);
  if (value.primaryId)
    *dest.mutable_primary_id() = std::move(*value.primaryId);
  auto moveOtherUids = MoveElements(value.otherUids);
  dest.mutable_other_uids()->Assign(moveOtherUids.begin(), moveOtherUids.end());

  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_groups(),
    std::move(value.groups));

}

}
