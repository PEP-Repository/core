#include <pep/accessmanager/AmaSerializers.hpp>

#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

AmaCreateColumn Serializer<AmaCreateColumn>::fromProtocolBuffer(proto::AmaCreateColumn&& source) const {
  return AmaCreateColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumn>::moveIntoProtocolBuffer(proto::AmaCreateColumn& dest, AmaCreateColumn value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaRemoveColumn Serializer<AmaRemoveColumn>::fromProtocolBuffer(proto::AmaRemoveColumn&& source) const {
  return AmaRemoveColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumn>::moveIntoProtocolBuffer(proto::AmaRemoveColumn& dest, AmaRemoveColumn value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaCreateColumnGroup Serializer<AmaCreateColumnGroup>::fromProtocolBuffer(proto::AmaCreateColumnGroup&& source) const {
  return AmaCreateColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumnGroup>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroup& dest, AmaCreateColumnGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaRemoveColumnGroup Serializer<AmaRemoveColumnGroup>::fromProtocolBuffer(proto::AmaRemoveColumnGroup&& source) const {
  return AmaRemoveColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumnGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroup& dest, AmaRemoveColumnGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaAddColumnToGroup Serializer<AmaAddColumnToGroup>::fromProtocolBuffer(proto::AmaAddColumnToGroup&& source) const {
  return AmaAddColumnToGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaAddColumnToGroup>::moveIntoProtocolBuffer(proto::AmaAddColumnToGroup& dest, AmaAddColumnToGroup value) const {
  *dest.mutable_column() = std::move(value.mColumn);
  *dest.mutable_column_group() = std::move(value.mColumnGroup);
}

AmaRemoveColumnFromGroup Serializer<AmaRemoveColumnFromGroup>::fromProtocolBuffer(proto::AmaRemoveColumnFromGroup&& source) const {
  return AmaRemoveColumnFromGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaRemoveColumnFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnFromGroup& dest, AmaRemoveColumnFromGroup value) const {
  *dest.mutable_column() = std::move(value.mColumn);
  *dest.mutable_column_group() = std::move(value.mColumnGroup);
}

AmaCreateParticipantGroup Serializer<AmaCreateParticipantGroup>::fromProtocolBuffer(proto::AmaCreateParticipantGroup&& source) const {
  return AmaCreateParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateParticipantGroup>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroup& dest, AmaCreateParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaRemoveParticipantGroup Serializer<AmaRemoveParticipantGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantGroup&& source) const {
  return AmaRemoveParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveParticipantGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroup& dest, AmaRemoveParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaAddParticipantToGroup Serializer<AmaAddParticipantToGroup>::fromProtocolBuffer(proto::AmaAddParticipantToGroup&& source) const {
  return AmaAddParticipantToGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaAddParticipantToGroup>::moveIntoProtocolBuffer(proto::AmaAddParticipantToGroup& dest, AmaAddParticipantToGroup value) const {
  *dest.mutable_group() = std::move(value.mParticipantGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.mParticipant.getValidElgamalEncryption());
}

AmaRemoveParticipantFromGroup Serializer<AmaRemoveParticipantFromGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantFromGroup&& source) const {
  return AmaRemoveParticipantFromGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaRemoveParticipantFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantFromGroup& dest, AmaRemoveParticipantFromGroup value) const {
  *dest.mutable_group() = std::move(value.mParticipantGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.mParticipant.getValidElgamalEncryption());
}

AmaCreateColumnGroupAccessRule Serializer<AmaCreateColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateColumnGroupAccessRule&& source) const {
  return AmaCreateColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroupAccessRule& dest, AmaCreateColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.mColumnGroup);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

AmaRemoveColumnGroupAccessRule Serializer<AmaRemoveColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule&& source) const {
  return AmaRemoveColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule& dest, AmaRemoveColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.mColumnGroup);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

AmaCreateParticipantGroupAccessRule Serializer<AmaCreateParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule&& source) const {
  return AmaCreateParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule& dest, AmaCreateParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.mParticipantGroup);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

AmaRemoveParticipantGroupAccessRule Serializer<AmaRemoveParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule&& source) const {
  return AmaRemoveParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule& dest, AmaRemoveParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.mParticipantGroup);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

AmaMutationRequest Serializer<AmaMutationRequest>::fromProtocolBuffer(proto::AmaMutationRequest&& source) const {
  AmaMutationRequest result;
  result.mForceColumnGroupRemoval = source.force_column_group_removal();
  result.mForceParticipantGroupRemoval = source.force_participant_group_removal();
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateColumn,
    std::move(*source.mutable_create_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveColumn,
    std::move(*source.mutable_remove_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateColumnGroup,
    std::move(*source.mutable_create_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveColumnGroup,
    std::move(*source.mutable_remove_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mAddColumnToGroup,
    std::move(*source.mutable_add_column_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveColumnFromGroup,
    std::move(*source.mutable_remove_column_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateParticipantGroup,
    std::move(*source.mutable_create_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveParticipantGroup,
    std::move(*source.mutable_remove_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mAddParticipantToGroup,
    std::move(*source.mutable_add_participant_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveParticipantFromGroup,
    std::move(*source.mutable_remove_participant_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateColumnGroupAccessRule,
    std::move(*source.mutable_create_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveColumnGroupAccessRule,
    std::move(*source.mutable_remove_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mCreateParticipantGroupAccessRule,
    std::move(*source.mutable_create_participant_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mRemoveParticipantGroupAccessRule,
    std::move(*source.mutable_remove_participant_group_access_rule()));
  return result;
}

void Serializer<AmaMutationRequest>::moveIntoProtocolBuffer(proto::AmaMutationRequest& dest, AmaMutationRequest value) const {
  dest.set_force_column_group_removal(value.mForceColumnGroupRemoval);
  dest.set_force_participant_group_removal(value.mForceParticipantGroupRemoval);
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_column(),
    std::move(value.mCreateColumn));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_column(),
    std::move(value.mRemoveColumn));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group(),
    std::move(value.mCreateColumnGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group(),
    std::move(value.mRemoveColumnGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_add_column_to_group(),
    std::move(value.mAddColumnToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_from_group(),
    std::move(value.mRemoveColumnFromGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_participant_group(),
    std::move(value.mCreateParticipantGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_group(),
    std::move(value.mRemoveParticipantGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_participant_to_group(),
    std::move(value.mAddParticipantToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_from_group(),
    std::move(value.mRemoveParticipantFromGroup));

  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group_access_rule(),
    std::move(value.mCreateColumnGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group_access_rule(),
    std::move(value.mRemoveColumnGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_participant_group_access_rule(),
    std::move(value.mCreateParticipantGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_participant_group_access_rule(),
    std::move(value.mRemoveParticipantGroupAccessRule));
}

AmaQueryResponse Serializer<AmaQueryResponse>::fromProtocolBuffer(proto::AmaQueryResponse&& source) const {
  AmaQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mColumns,
    std::move(*source.mutable_columns()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mColumnGroups,
    std::move(*source.mutable_column_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mColumnGroupAccessRules,
    std::move(*source.mutable_column_group_access_rules()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mParticipantGroups,
    std::move(*source.mutable_participant_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mParticipantGroupAccessRules,
    std::move(*source.mutable_participant_group_access_rules()));
  return result;
}

void Serializer<AmaQueryResponse>::moveIntoProtocolBuffer(proto::AmaQueryResponse& dest, AmaQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_columns(),
    std::move(value.mColumns));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_column_groups(),
    std::move(value.mColumnGroups));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_column_group_access_rules(),
    std::move(value.mColumnGroupAccessRules));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_groups(),
    std::move(value.mParticipantGroups));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_group_access_rules(),
    std::move(value.mParticipantGroupAccessRules));
}

AmaQuery Serializer<AmaQuery>::fromProtocolBuffer(proto::AmaQuery&& source) const {
  return {
    .mAt = source.has_at()
        ? std::optional(Serialization::FromProtocolBuffer(std::move(*source.mutable_at())))
        : std::nullopt,
    .mColumnFilter = std::move(*source.mutable_column_filter()),
    .mColumnGroupFilter = std::move(*source.mutable_column_group_filter()),
    .mParticipantGroupFilter = std::move(*source.mutable_participant_group_filter()),
    .mUserGroupFilter = std::move(*source.mutable_user_group_filter()),
    .mColumnGroupModeFilter = std::move(*source.mutable_column_group_mode_filter()),
    .mParticipantGroupModeFilter = std::move(*source.mutable_participant_group_mode_filter())
  };
}

void Serializer<AmaQuery>::moveIntoProtocolBuffer(proto::AmaQuery& dest, AmaQuery value) const {
  if (value.mAt) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), *value.mAt);
  }
  *dest.mutable_column_filter() = std::move(value.mColumnFilter);
  *dest.mutable_column_group_filter() = std::move(value.mColumnGroupFilter);
  *dest.mutable_participant_group_filter() = std::move(value.mParticipantGroupFilter);
  *dest.mutable_user_group_filter() = std::move(value.mUserGroupFilter);
  *dest.mutable_column_group_mode_filter() = std::move(value.mColumnGroupModeFilter);
  *dest.mutable_participant_group_mode_filter() = std::move(value.mParticipantGroupModeFilter);
}

AmaQRColumn Serializer<AmaQRColumn>::fromProtocolBuffer(proto::AmaQRColumn&& source) const {
  return AmaQRColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaQRColumn>::moveIntoProtocolBuffer(proto::AmaQRColumn& dest, AmaQRColumn value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaQRParticipantGroup Serializer<AmaQRParticipantGroup>::fromProtocolBuffer(proto::AmaQRParticipantGroup&& source) const {
  return AmaQRParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaQRParticipantGroup>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroup& dest, AmaQRParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
}

AmaQRColumnGroup Serializer<AmaQRColumnGroup>::fromProtocolBuffer(proto::AmaQRColumnGroup&& source) const {
  std::vector<std::string> columns;
  columns.reserve(static_cast<size_t>(source.columns().size()));
  for (auto& x : *source.mutable_columns())
    columns.push_back(std::move(x));
  return AmaQRColumnGroup(
    std::move(*source.mutable_name()),
    std::move(columns));
}

void Serializer<AmaQRColumnGroup>::moveIntoProtocolBuffer(proto::AmaQRColumnGroup& dest, AmaQRColumnGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
  dest.mutable_columns()->Reserve(static_cast<int>(value.mColumns.size()));
  for (auto& x : value.mColumns)
    dest.add_columns(std::move(x));
}

AmaQRColumnGroupAccessRule Serializer<AmaQRColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaQRColumnGroupAccessRule&& source) const {
  return AmaQRColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_access_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaQRColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaQRColumnGroupAccessRule& dest, AmaQRColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.mColumnGroup);
  *dest.mutable_access_group() = std::move(value.mAccessGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

AmaQRParticipantGroupAccessRule Serializer<AmaQRParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaQRParticipantGroupAccessRule&& source) const {
  return AmaQRParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaQRParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroupAccessRule& dest, AmaQRParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.mParticipantGroup);
  *dest.mutable_user_group() = std::move(value.mUserGroup);
  *dest.mutable_mode() = std::move(value.mMode);
}

}
