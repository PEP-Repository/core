#include <pep/accessmanager/UserSerializers.hpp>
#include <pep/auth/UserGroupSerializers.hpp>

namespace pep {

CreateUser Serializer<CreateUser>::fromProtocolBuffer(proto::CreateUser&& source) const {
  return CreateUser(std::move(*source.mutable_uid()));
}

void Serializer<CreateUser>::moveIntoProtocolBuffer(proto::CreateUser& dest, CreateUser value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

RemoveUser Serializer<RemoveUser>::fromProtocolBuffer(proto::RemoveUser&& source) const {
  return RemoveUser(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUser>::moveIntoProtocolBuffer(proto::RemoveUser& dest, RemoveUser value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

AddUserIdentifier Serializer<AddUserIdentifier>::fromProtocolBuffer(proto::AddUserIdentifier&& source) const {
  return AddUserIdentifier(std::move(*source.mutable_existinguid()), std::move(*source.mutable_newuid()), source.is_primary_id(), source.is_display_id());
}

void Serializer<AddUserIdentifier>::moveIntoProtocolBuffer(proto::AddUserIdentifier& dest, AddUserIdentifier value) const {
  *dest.mutable_existinguid() = std::move(value.mExistingUid);
  *dest.mutable_newuid() = std::move(value.mNewUid);
  dest.set_is_primary_id(value.mIsPrimaryId);
  dest.set_is_display_id(value.mIsDisplayId);
}

UpdateUserIdentifier Serializer<UpdateUserIdentifier>::fromProtocolBuffer(proto::UpdateUserIdentifier&& source) const {
  UpdateUserIdentifier result;
  result.mUid = std::move(*source.mutable_uid());
  if (source.has_is_primary_id()) {
    result.mIsPrimaryId = source.is_primary_id();
  }
  if (source.has_is_display_id()) {
    result.mIsDisplayId = source.is_display_id();
  }
  return result;
}

void Serializer<UpdateUserIdentifier>::moveIntoProtocolBuffer(proto::UpdateUserIdentifier& dest, UpdateUserIdentifier value) const {
  *dest.mutable_uid() = std::move(value.mUid);
  if (value.mIsPrimaryId)
    dest.set_is_primary_id(*value.mIsPrimaryId);
  if (value.mIsDisplayId)
    dest.set_is_display_id(*value.mIsDisplayId);
}

RemoveUserIdentifier Serializer<RemoveUserIdentifier>::fromProtocolBuffer(proto::RemoveUserIdentifier&& source) const {
  return RemoveUserIdentifier(std::move(*source.mutable_uid()));
}

void Serializer<RemoveUserIdentifier>::moveIntoProtocolBuffer(proto::RemoveUserIdentifier& dest, RemoveUserIdentifier value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

CreateUserGroup Serializer<CreateUserGroup>::fromProtocolBuffer(proto::CreateUserGroup&& source) const {
  return CreateUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<CreateUserGroup>::moveIntoProtocolBuffer(proto::CreateUserGroup& dest, CreateUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.mUserGroup));
}

RemoveUserGroup Serializer<RemoveUserGroup>::fromProtocolBuffer(proto::RemoveUserGroup&& source) const {
  return RemoveUserGroup(std::move(*source.mutable_name()));
}

void Serializer<RemoveUserGroup>::moveIntoProtocolBuffer(proto::RemoveUserGroup& dest, RemoveUserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

ModifyUserGroup Serializer<ModifyUserGroup>::fromProtocolBuffer(proto::ModifyUserGroup&& source) const {
  return ModifyUserGroup(Serialization::FromProtocolBuffer(std::move(*source.mutable_user_group())));
}

void Serializer<ModifyUserGroup>::moveIntoProtocolBuffer(proto::ModifyUserGroup& dest, ModifyUserGroup value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_user_group(), std::move(value.mUserGroup));
}

AddUserToGroup Serializer<AddUserToGroup>::fromProtocolBuffer(proto::AddUserToGroup&& source) const {
  return AddUserToGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group())
  );
}

void Serializer<AddUserToGroup>::moveIntoProtocolBuffer(proto::AddUserToGroup& dest, AddUserToGroup value) const {
  *dest.mutable_uid() = std::move(value.mUid);
  *dest.mutable_group() = std::move(value.mGroup);
}

RemoveUserFromGroup Serializer<RemoveUserFromGroup>::fromProtocolBuffer(proto::RemoveUserFromGroup&& source) const {
  return RemoveUserFromGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group()),
    source.block_tokens()
  );
}

void Serializer<RemoveUserFromGroup>::moveIntoProtocolBuffer(proto::RemoveUserFromGroup& dest, RemoveUserFromGroup value) const {
  *dest.mutable_uid() = std::move(value.mUid);
  *dest.mutable_group() = std::move(value.mGroup);
  dest.set_block_tokens(value.mBlockTokens);
}

UserMutationRequest Serializer<UserMutationRequest>::fromProtocolBuffer(proto::UserMutationRequest&& source) const {
  UserMutationRequest result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateUser,
    std::move(*source.mutable_create_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUser,
    std::move(*source.mutable_remove_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mAddUserIdentifier,
    std::move(*source.mutable_add_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUserIdentifier,
    std::move(*source.mutable_remove_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mUpdateUserIdentifier,
    std::move(*source.mutable_update_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateUserGroup,
    std::move(*source.mutable_create_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUserGroup,
    std::move(*source.mutable_remove_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mModifyUserGroup,
    std::move(*source.mutable_modify_user_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mAddUserToGroup,
    std::move(*source.mutable_add_user_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUserFromGroup,
    std::move(*source.mutable_remove_user_from_group()));
  return result;
}

void Serializer<UserMutationRequest>::moveIntoProtocolBuffer(proto::UserMutationRequest& dest, UserMutationRequest value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user(),
    std::move(value.mCreateUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user(),
    std::move(value.mRemoveUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_identifier(),
    std::move(value.mAddUserIdentifier));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_update_user_identifier(),
    std::move(value.mUpdateUserIdentifier));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_identifier(),
    std::move(value.mRemoveUserIdentifier));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user_group(),
    std::move(value.mCreateUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_group(),
    std::move(value.mRemoveUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_modify_user_group(),
    std::move(value.mModifyUserGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_to_group(),
    std::move(value.mAddUserToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user_from_group(),
    std::move(value.mRemoveUserFromGroup));
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
  UserQuery result;
  result.mAt = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_at()));
  result.mGroupFilter = std::move(*source.mutable_group_filter());
  result.mUserFilter = std::move(*source.mutable_user_filter());
  return result;
}

void Serializer<UserQuery>::moveIntoProtocolBuffer(proto::UserQuery& dest, UserQuery value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), value.mAt);
  *dest.mutable_group_filter() = std::move(value.mGroupFilter);
  *dest.mutable_user_filter() = std::move(value.mUserFilter);
}

QRUser Serializer<QRUser>::fromProtocolBuffer(proto::QRUser&& source) const {
  std::vector<std::string> uids;
  uids.reserve(static_cast<size_t>(source.uids().size()));
  for (auto& x : *source.mutable_uids())
    uids.push_back(std::move(x));

  std::vector<std::string> groups;
  groups.reserve(static_cast<size_t>(source.groups().size()));
  for (auto& x : *source.mutable_groups())
    groups.push_back(std::move(x));
  return QRUser(std::move(uids), std::move(groups));
}

void Serializer<QRUser>::moveIntoProtocolBuffer(proto::QRUser& dest, QRUser value) const {
  dest.mutable_uids()->Reserve(static_cast<int>(value.mUids.size()));
  for (auto& x : value.mUids)
    dest.add_uids(std::move(x));

  dest.mutable_groups()->Reserve(static_cast<int>(value.mGroups.size()));
  for (auto& x : value.mGroups)
    dest.add_groups(std::move(x));
}

}
