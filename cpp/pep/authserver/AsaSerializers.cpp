#include <pep/authserver/AsaSerializers.hpp>

namespace pep {

AsaTokenRequest Serializer<AsaTokenRequest>::fromProtocolBuffer(proto::AsaTokenRequest&& source) const {
  return AsaTokenRequest(
    std::move(*source.mutable_subject()),
    std::move(*source.mutable_group()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_expirationtime()))
  );
}

void Serializer<AsaTokenRequest>::moveIntoProtocolBuffer(proto::AsaTokenRequest& dest, AsaTokenRequest value) const {
  *dest.mutable_subject() = std::move(value.mSubject);
  *dest.mutable_group() = std::move(value.mGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_expirationtime(), value.mExpirationTime);
}

AsaTokenResponse Serializer<AsaTokenResponse>::fromProtocolBuffer(proto::AsaTokenResponse&& source) const {
  return AsaTokenResponse(std::move(*source.mutable_token()));
}

void Serializer<AsaTokenResponse>::moveIntoProtocolBuffer(proto::AsaTokenResponse& dest, AsaTokenResponse value) const {
  *dest.mutable_token() = std::move(value.mToken);
}

AsaCreateUser Serializer<AsaCreateUser>::fromProtocolBuffer(proto::AsaCreateUser&& source) const {
  return AsaCreateUser(std::move(*source.mutable_uid()));
}

void Serializer<AsaCreateUser>::moveIntoProtocolBuffer(proto::AsaCreateUser& dest, AsaCreateUser value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

AsaRemoveUser Serializer<AsaRemoveUser>::fromProtocolBuffer(proto::AsaRemoveUser&& source) const {
  return AsaRemoveUser(std::move(*source.mutable_uid()));
}

void Serializer<AsaRemoveUser>::moveIntoProtocolBuffer(proto::AsaRemoveUser& dest, AsaRemoveUser value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

AsaAddUserIdentifier Serializer<AsaAddUserIdentifier>::fromProtocolBuffer(proto::AsaAddUserIdentifier&& source) const {
  return AsaAddUserIdentifier(std::move(*source.mutable_existinguid()), std::move(*source.mutable_newuid()));
}

void Serializer<AsaAddUserIdentifier>::moveIntoProtocolBuffer(proto::AsaAddUserIdentifier& dest, AsaAddUserIdentifier value) const {
  *dest.mutable_existinguid() = std::move(value.mExistingUid);
  *dest.mutable_newuid() = std::move(value.mNewUid);
}

AsaRemoveUserIdentifier Serializer<AsaRemoveUserIdentifier>::fromProtocolBuffer(proto::AsaRemoveUserIdentifier&& source) const {
  return AsaRemoveUserIdentifier(std::move(*source.mutable_uid()));
}

void Serializer<AsaRemoveUserIdentifier>::moveIntoProtocolBuffer(proto::AsaRemoveUserIdentifier& dest, AsaRemoveUserIdentifier value) const {
  *dest.mutable_uid() = std::move(value.mUid);
}

UserGroupProperties Serializer<UserGroupProperties>::fromProtocolBuffer(proto::UserGroupProperties&& source) const {
  std::optional<std::chrono::seconds> maxAuthValidity;
  if (source.optional_max_auth_validity_seconds_case() == proto::UserGroupProperties::kMaxAuthValiditySeconds) {
    maxAuthValidity = std::chrono::seconds(source.max_auth_validity_seconds());
  }
  return UserGroupProperties(maxAuthValidity);
}

void Serializer<UserGroupProperties>::moveIntoProtocolBuffer(proto::UserGroupProperties& dest, UserGroupProperties value) const {
  if (value.mMaxAuthValidity) {
    dest.set_max_auth_validity_seconds(static_cast<uint64_t>(value.mMaxAuthValidity->count()));
  }
}

AsaCreateUserGroup Serializer<AsaCreateUserGroup>::fromProtocolBuffer(proto::AsaCreateUserGroup&& source) const {
  return AsaCreateUserGroup(std::move(*source.mutable_name()), Serialization::FromProtocolBuffer(std::move(*source.mutable_properties())));
}

void Serializer<AsaCreateUserGroup>::moveIntoProtocolBuffer(proto::AsaCreateUserGroup& dest, AsaCreateUserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_properties(), std::move(value.mProperties));
}

AsaRemoveUserGroup Serializer<AsaRemoveUserGroup>::fromProtocolBuffer(proto::AsaRemoveUserGroup&& source) const {
  return AsaRemoveUserGroup(std::move(*source.mutable_name()));
}

void Serializer<AsaRemoveUserGroup>::moveIntoProtocolBuffer(proto::AsaRemoveUserGroup& dest, AsaRemoveUserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AsaModifyUserGroup Serializer<AsaModifyUserGroup>::fromProtocolBuffer(proto::AsaModifyUserGroup&& source) const {
  return AsaModifyUserGroup(std::move(*source.mutable_name()), Serialization::FromProtocolBuffer(std::move(*source.mutable_properties())));
}

void Serializer<AsaModifyUserGroup>::moveIntoProtocolBuffer(proto::AsaModifyUserGroup& dest, AsaModifyUserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_properties(), std::move(value.mProperties));
}

AsaAddUserToGroup Serializer<AsaAddUserToGroup>::fromProtocolBuffer(proto::AsaAddUserToGroup&& source) const {
  return AsaAddUserToGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group())
  );
}

void Serializer<AsaAddUserToGroup>::moveIntoProtocolBuffer(proto::AsaAddUserToGroup& dest, AsaAddUserToGroup value) const {
  *dest.mutable_uid() = std::move(value.mUid);
  *dest.mutable_group() = std::move(value.mGroup);
}

AsaRemoveUserFromGroup Serializer<AsaRemoveUserFromGroup>::fromProtocolBuffer(proto::AsaRemoveUserFromGroup&& source) const {
  return AsaRemoveUserFromGroup(
    std::move(*source.mutable_uid()),
    std::move(*source.mutable_group())
  );
}

void Serializer<AsaRemoveUserFromGroup>::moveIntoProtocolBuffer(proto::AsaRemoveUserFromGroup& dest, AsaRemoveUserFromGroup value) const {
  *dest.mutable_uid() = std::move(value.mUid);
  *dest.mutable_group() = std::move(value.mGroup);
}

AsaMutationRequest Serializer<AsaMutationRequest>::fromProtocolBuffer(proto::AsaMutationRequest&& source) const {
  AsaMutationRequest result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateUser,
    std::move(*source.mutable_create_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUser,
    std::move(*source.mutable_remove_user()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mAddUserIdentifier,
    std::move(*source.mutable_add_user_identifier()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveUserIdentifier,
    std::move(*source.mutable_remove_user_identifier()));
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

void Serializer<AsaMutationRequest>::moveIntoProtocolBuffer(proto::AsaMutationRequest& dest, AsaMutationRequest value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_user(),
    std::move(value.mCreateUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_user(),
    std::move(value.mRemoveUser));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_user_identifier(),
    std::move(value.mAddUserIdentifier));
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

AsaQueryResponse Serializer<AsaQueryResponse>::fromProtocolBuffer(proto::AsaQueryResponse&& source) const {
  AsaQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mUsers,
    std::move(*source.mutable_users()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mUserGroups,
    std::move(*source.mutable_user_groups()));
  return result;
}

void Serializer<AsaQueryResponse>::moveIntoProtocolBuffer(proto::AsaQueryResponse& dest, AsaQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_users(),
    std::move(value.mUsers));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_user_groups(),
    std::move(value.mUserGroups));
}

AsaQuery Serializer<AsaQuery>::fromProtocolBuffer(proto::AsaQuery&& source) const {
  AsaQuery result;
  result.mAt = Serialization::FromProtocolBuffer(
    std::move(*source.mutable_at()));
  result.mGroupFilter = std::move(*source.mutable_group_filter());
  result.mUserFilter = std::move(*source.mutable_user_filter());
  return result;
}

void Serializer<AsaQuery>::moveIntoProtocolBuffer(proto::AsaQuery& dest, AsaQuery value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), value.mAt);
  *dest.mutable_group_filter() = std::move(value.mGroupFilter);
  *dest.mutable_user_filter() = std::move(value.mUserFilter);
}

AsaQRUserGroup Serializer<AsaQRUserGroup>::fromProtocolBuffer(proto::AsaQRUserGroup&& source) const {
  return AsaQRUserGroup(std::move(*source.mutable_name()), Serialization::FromProtocolBuffer(std::move(*source.mutable_properties())));
}

void Serializer<AsaQRUserGroup>::moveIntoProtocolBuffer(proto::AsaQRUserGroup& dest, AsaQRUserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_properties(), std::move(value.mProperties));
}

AsaQRUser Serializer<AsaQRUser>::fromProtocolBuffer(proto::AsaQRUser&& source) const {
  std::vector<std::string> uids;
  uids.reserve(static_cast<size_t>(source.uids().size()));
  for (auto& x : *source.mutable_uids())
    uids.push_back(std::move(x));

  std::vector<std::string> groups;
  groups.reserve(static_cast<size_t>(source.groups().size()));
  for (auto& x : *source.mutable_groups())
    groups.push_back(std::move(x));
  return AsaQRUser(std::move(uids), std::move(groups));
}

void Serializer<AsaQRUser>::moveIntoProtocolBuffer(proto::AsaQRUser& dest, AsaQRUser value) const {
  dest.mutable_uids()->Reserve(static_cast<int>(value.mUids.size()));
  for (auto& x : value.mUids)
    dest.add_uids(std::move(x));

  dest.mutable_groups()->Reserve(static_cast<int>(value.mGroups.size()));
  for (auto& x : value.mGroups)
    dest.add_groups(std::move(x));
}

}
