#include <pep/accessmanager/AmaSerializers.hpp>

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

AmaCreateColumn Serializer<AmaCreateColumn>::fromProtocolBuffer(proto::AmaCreateColumn&& source) const {
  return AmaCreateColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumn>::moveIntoProtocolBuffer(proto::AmaCreateColumn& dest, AmaCreateColumn value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaRemoveColumn Serializer<AmaRemoveColumn>::fromProtocolBuffer(proto::AmaRemoveColumn&& source) const {
  return AmaRemoveColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumn>::moveIntoProtocolBuffer(proto::AmaRemoveColumn& dest, AmaRemoveColumn value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaCreateColumnGroup Serializer<AmaCreateColumnGroup>::fromProtocolBuffer(proto::AmaCreateColumnGroup&& source) const {
  return AmaCreateColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumnGroup>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroup& dest, AmaCreateColumnGroup value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaRemoveColumnGroup Serializer<AmaRemoveColumnGroup>::fromProtocolBuffer(proto::AmaRemoveColumnGroup&& source) const {
  return AmaRemoveColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumnGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroup& dest, AmaRemoveColumnGroup value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaAddColumnToGroup Serializer<AmaAddColumnToGroup>::fromProtocolBuffer(proto::AmaAddColumnToGroup&& source) const {
  return AmaAddColumnToGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaAddColumnToGroup>::moveIntoProtocolBuffer(proto::AmaAddColumnToGroup& dest, AmaAddColumnToGroup value) const {
  *dest.mutable_column() = std::move(value.column);
  *dest.mutable_column_group() = std::move(value.columnGroup);
}

AmaRemoveColumnFromGroup Serializer<AmaRemoveColumnFromGroup>::fromProtocolBuffer(proto::AmaRemoveColumnFromGroup&& source) const {
  return AmaRemoveColumnFromGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaRemoveColumnFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnFromGroup& dest, AmaRemoveColumnFromGroup value) const {
  *dest.mutable_column() = std::move(value.column);
  *dest.mutable_column_group() = std::move(value.columnGroup);
}

AmaCreateParticipantGroup Serializer<AmaCreateParticipantGroup>::fromProtocolBuffer(proto::AmaCreateParticipantGroup&& source) const {
  return AmaCreateParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateParticipantGroup>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroup& dest, AmaCreateParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaRemoveParticipantGroup Serializer<AmaRemoveParticipantGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantGroup&& source) const {
  return AmaRemoveParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveParticipantGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroup& dest, AmaRemoveParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaAddParticipantToGroup Serializer<AmaAddParticipantToGroup>::fromProtocolBuffer(proto::AmaAddParticipantToGroup&& source) const {
  return AmaAddParticipantToGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaAddParticipantToGroup>::moveIntoProtocolBuffer(proto::AmaAddParticipantToGroup& dest, AmaAddParticipantToGroup value) const {
  *dest.mutable_group() = std::move(value.participantGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.participant.getValidElgamalEncryption());
}

AmaRemoveParticipantFromGroup Serializer<AmaRemoveParticipantFromGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantFromGroup&& source) const {
  return AmaRemoveParticipantFromGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaRemoveParticipantFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantFromGroup& dest, AmaRemoveParticipantFromGroup value) const {
  *dest.mutable_group() = std::move(value.participantGroup);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.participant.getValidElgamalEncryption());
}

AmaCreateColumnGroupAccessRule Serializer<AmaCreateColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateColumnGroupAccessRule&& source) const {
  return AmaCreateColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroupAccessRule& dest, AmaCreateColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.columnGroup);
  *dest.mutable_user_group() = std::move(value.userGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

AmaRemoveColumnGroupAccessRule Serializer<AmaRemoveColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule&& source) const {
  return AmaRemoveColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule& dest, AmaRemoveColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.columnGroup);
  *dest.mutable_user_group() = std::move(value.userGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

AmaCreateParticipantGroupAccessRule Serializer<AmaCreateParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule&& source) const {
  return AmaCreateParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule& dest, AmaCreateParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup);
  *dest.mutable_user_group() = std::move(value.userGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

AmaRemoveParticipantGroupAccessRule Serializer<AmaRemoveParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule&& source) const {
  return AmaRemoveParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule& dest, AmaRemoveParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup);
  *dest.mutable_user_group() = std::move(value.userGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

AmaMutationRequest Serializer<AmaMutationRequest>::fromProtocolBuffer(proto::AmaMutationRequest&& source) const {
  AmaMutationRequest result;
  result.forceColumnGroupRemoval = source.force_column_group_removal();
  result.forceParticipantGroupRemoval = source.force_participant_group_removal();
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumn,
    std::move(*source.mutable_create_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumn,
    std::move(*source.mutable_remove_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumnGroup,
    std::move(*source.mutable_create_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnGroup,
    std::move(*source.mutable_remove_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addColumnToGroup,
    std::move(*source.mutable_add_column_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnFromGroup,
    std::move(*source.mutable_remove_column_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createParticipantGroup,
    std::move(*source.mutable_create_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantGroup,
    std::move(*source.mutable_remove_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addParticipantToGroup,
    std::move(*source.mutable_add_participant_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantFromGroup,
    std::move(*source.mutable_remove_participant_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumnGroupAccessRule,
    std::move(*source.mutable_create_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnGroupAccessRule,
    std::move(*source.mutable_remove_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createParticipantGroupAccessRule,
    std::move(*source.mutable_create_participant_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantGroupAccessRule,
    std::move(*source.mutable_remove_participant_group_access_rule()));
  return result;
}

void Serializer<AmaMutationRequest>::moveIntoProtocolBuffer(proto::AmaMutationRequest& dest, AmaMutationRequest value) const {
  dest.set_force_column_group_removal(value.forceColumnGroupRemoval);
  dest.set_force_participant_group_removal(value.forceParticipantGroupRemoval);
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_column(),
    std::move(value.createColumn));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_column(),
    std::move(value.removeColumn));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group(),
    std::move(value.createColumnGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group(),
    std::move(value.removeColumnGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_add_column_to_group(),
    std::move(value.addColumnToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_from_group(),
    std::move(value.removeColumnFromGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_participant_group(),
    std::move(value.createParticipantGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_group(),
    std::move(value.removeParticipantGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_participant_to_group(),
    std::move(value.addParticipantToGroup));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_from_group(),
    std::move(value.removeParticipantFromGroup));

  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group_access_rule(),
    std::move(value.createColumnGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group_access_rule(),
    std::move(value.removeColumnGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_participant_group_access_rule(),
    std::move(value.createParticipantGroupAccessRule));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_participant_group_access_rule(),
    std::move(value.removeParticipantGroupAccessRule));
}

AmaQueryResponse Serializer<AmaQueryResponse>::fromProtocolBuffer(proto::AmaQueryResponse&& source) const {
  AmaQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.columns,
    std::move(*source.mutable_columns()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.columnGroups,
    std::move(*source.mutable_column_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.columnGroupAccessRules,
    std::move(*source.mutable_column_group_access_rules()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.participantGroups,
    std::move(*source.mutable_participant_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.participantGroupAccessRules,
    std::move(*source.mutable_participant_group_access_rules()));
  return result;
}

void Serializer<AmaQueryResponse>::moveIntoProtocolBuffer(proto::AmaQueryResponse& dest, AmaQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_columns(),
    std::move(value.columns));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_column_groups(),
    std::move(value.columnGroups));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_column_group_access_rules(),
    std::move(value.columnGroupAccessRules));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_groups(),
    std::move(value.participantGroups));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_group_access_rules(),
    std::move(value.participantGroupAccessRules));
}

AmaQuery Serializer<AmaQuery>::fromProtocolBuffer(proto::AmaQuery&& source) const {
  return {
    .at = source.has_at()
        ? std::optional(Serialization::FromProtocolBuffer(std::move(*source.mutable_at())))
        : std::nullopt,
    .columnFilter = std::move(*source.mutable_column_filter()),
    .columnGroupFilter = std::move(*source.mutable_column_group_filter()),
    .participantGroupFilter = std::move(*source.mutable_participant_group_filter()),
    .userGroupFilter = std::move(*source.mutable_user_group_filter()),
    .columnGroupModeFilter = std::move(*source.mutable_column_group_mode_filter()),
    .participantGroupModeFilter = std::move(*source.mutable_participant_group_mode_filter())
  };
}

void Serializer<AmaQuery>::moveIntoProtocolBuffer(proto::AmaQuery& dest, AmaQuery value) const {
  if (value.at) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), *value.at);
  }
  *dest.mutable_column_filter() = std::move(value.columnFilter);
  *dest.mutable_column_group_filter() = std::move(value.columnGroupFilter);
  *dest.mutable_participant_group_filter() = std::move(value.participantGroupFilter);
  *dest.mutable_user_group_filter() = std::move(value.userGroupFilter);
  *dest.mutable_column_group_mode_filter() = std::move(value.columnGroupModeFilter);
  *dest.mutable_participant_group_mode_filter() = std::move(value.participantGroupModeFilter);
}

AmaQRColumn Serializer<AmaQRColumn>::fromProtocolBuffer(proto::AmaQRColumn&& source) const {
  return AmaQRColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaQRColumn>::moveIntoProtocolBuffer(proto::AmaQRColumn& dest, AmaQRColumn value) const {
  *dest.mutable_name() = std::move(value.name);
}

AmaQRParticipantGroup Serializer<AmaQRParticipantGroup>::fromProtocolBuffer(proto::AmaQRParticipantGroup&& source) const {
  return AmaQRParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaQRParticipantGroup>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroup& dest, AmaQRParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name);
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
  *dest.mutable_name() = std::move(value.name);
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns.size()));
  for (auto& x : value.columns)
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
  *dest.mutable_column_group() = std::move(value.columnGroup);
  *dest.mutable_access_group() = std::move(value.accessGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

AmaQRParticipantGroupAccessRule Serializer<AmaQRParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaQRParticipantGroupAccessRule&& source) const {
  return AmaQRParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaQRParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroupAccessRule& dest, AmaQRParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup);
  *dest.mutable_user_group() = std::move(value.userGroup);
  *dest.mutable_mode() = std::move(value.mode);
}

}
