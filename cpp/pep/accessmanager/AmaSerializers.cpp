#include <pep/accessmanager/AmaSerializers.hpp>

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

AmaCreateColumn Serializer<AmaCreateColumn>::fromProtocolBuffer(proto::AmaCreateColumn&& source) const {
  return AmaCreateColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumn>::moveIntoProtocolBuffer(proto::AmaCreateColumn& dest, AmaCreateColumn value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaRemoveColumn Serializer<AmaRemoveColumn>::fromProtocolBuffer(proto::AmaRemoveColumn&& source) const {
  return AmaRemoveColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumn>::moveIntoProtocolBuffer(proto::AmaRemoveColumn& dest, AmaRemoveColumn value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaCreateColumnGroup Serializer<AmaCreateColumnGroup>::fromProtocolBuffer(proto::AmaCreateColumnGroup&& source) const {
  return AmaCreateColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateColumnGroup>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroup& dest, AmaCreateColumnGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaRemoveColumnGroup Serializer<AmaRemoveColumnGroup>::fromProtocolBuffer(proto::AmaRemoveColumnGroup&& source) const {
  return AmaRemoveColumnGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveColumnGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroup& dest, AmaRemoveColumnGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaAddColumnToGroup Serializer<AmaAddColumnToGroup>::fromProtocolBuffer(proto::AmaAddColumnToGroup&& source) const {
  return AmaAddColumnToGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaAddColumnToGroup>::moveIntoProtocolBuffer(proto::AmaAddColumnToGroup& dest, AmaAddColumnToGroup value) const {
  *dest.mutable_column() = std::move(value.column_);
  *dest.mutable_column_group() = std::move(value.columnGroup_);
}

AmaRemoveColumnFromGroup Serializer<AmaRemoveColumnFromGroup>::fromProtocolBuffer(proto::AmaRemoveColumnFromGroup&& source) const {
  return AmaRemoveColumnFromGroup(
    std::move(*source.mutable_column()),
    std::move(*source.mutable_column_group())
  );
}

void Serializer<AmaRemoveColumnFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveColumnFromGroup& dest, AmaRemoveColumnFromGroup value) const {
  *dest.mutable_column() = std::move(value.column_);
  *dest.mutable_column_group() = std::move(value.columnGroup_);
}

AmaCreateParticipantGroup Serializer<AmaCreateParticipantGroup>::fromProtocolBuffer(proto::AmaCreateParticipantGroup&& source) const {
  return AmaCreateParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaCreateParticipantGroup>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroup& dest, AmaCreateParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaRemoveParticipantGroup Serializer<AmaRemoveParticipantGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantGroup&& source) const {
  return AmaRemoveParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaRemoveParticipantGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroup& dest, AmaRemoveParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaAddParticipantToGroup Serializer<AmaAddParticipantToGroup>::fromProtocolBuffer(proto::AmaAddParticipantToGroup&& source) const {
  return AmaAddParticipantToGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaAddParticipantToGroup>::moveIntoProtocolBuffer(proto::AmaAddParticipantToGroup& dest, AmaAddParticipantToGroup value) const {
  *dest.mutable_group() = std::move(value.participantGroup_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.participant_.getValidElgamalEncryption());
}

AmaRemoveParticipantFromGroup Serializer<AmaRemoveParticipantFromGroup>::fromProtocolBuffer(proto::AmaRemoveParticipantFromGroup&& source) const {
  return AmaRemoveParticipantFromGroup(std::move(*source.mutable_group()), PolymorphicPseudonym(Serialization::FromProtocolBuffer(std::move(*source.mutable_participant()))));
}

void Serializer<AmaRemoveParticipantFromGroup>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantFromGroup& dest, AmaRemoveParticipantFromGroup value) const {
  *dest.mutable_group() = std::move(value.participantGroup_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_participant(), value.participant_.getValidElgamalEncryption());
}

AmaCreateColumnGroupAccessRule Serializer<AmaCreateColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateColumnGroupAccessRule&& source) const {
  return AmaCreateColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateColumnGroupAccessRule& dest, AmaCreateColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.columnGroup_);
  *dest.mutable_user_group() = std::move(value.userGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

AmaRemoveColumnGroupAccessRule Serializer<AmaRemoveColumnGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule&& source) const {
  return AmaRemoveColumnGroupAccessRule(
    std::move(*source.mutable_column_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveColumnGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveColumnGroupAccessRule& dest, AmaRemoveColumnGroupAccessRule value) const {
  *dest.mutable_column_group() = std::move(value.columnGroup_);
  *dest.mutable_user_group() = std::move(value.userGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

AmaCreateParticipantGroupAccessRule Serializer<AmaCreateParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule&& source) const {
  return AmaCreateParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaCreateParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaCreateParticipantGroupAccessRule& dest, AmaCreateParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup_);
  *dest.mutable_user_group() = std::move(value.userGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

AmaRemoveParticipantGroupAccessRule Serializer<AmaRemoveParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule&& source) const {
  return AmaRemoveParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaRemoveParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaRemoveParticipantGroupAccessRule& dest, AmaRemoveParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup_);
  *dest.mutable_user_group() = std::move(value.userGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

AmaMutationRequest Serializer<AmaMutationRequest>::fromProtocolBuffer(proto::AmaMutationRequest&& source) const {
  AmaMutationRequest result;
  result.forceColumnGroupRemoval_ = source.force_column_group_removal();
  result.forceParticipantGroupRemoval_ = source.force_participant_group_removal();
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumn_,
    std::move(*source.mutable_create_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumn_,
    std::move(*source.mutable_remove_column()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumnGroup_,
    std::move(*source.mutable_create_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnGroup_,
    std::move(*source.mutable_remove_column_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addColumnToGroup_,
    std::move(*source.mutable_add_column_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnFromGroup_,
    std::move(*source.mutable_remove_column_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createParticipantGroup_,
    std::move(*source.mutable_create_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantGroup_,
    std::move(*source.mutable_remove_participant_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.addParticipantToGroup_,
    std::move(*source.mutable_add_participant_to_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantFromGroup_,
    std::move(*source.mutable_remove_participant_from_group()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createColumnGroupAccessRule_,
    std::move(*source.mutable_create_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeColumnGroupAccessRule_,
    std::move(*source.mutable_remove_column_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.createParticipantGroupAccessRule_,
    std::move(*source.mutable_create_participant_group_access_rule()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.removeParticipantGroupAccessRule_,
    std::move(*source.mutable_remove_participant_group_access_rule()));
  return result;
}

void Serializer<AmaMutationRequest>::moveIntoProtocolBuffer(proto::AmaMutationRequest& dest, AmaMutationRequest value) const {
  dest.set_force_column_group_removal(value.forceColumnGroupRemoval_);
  dest.set_force_participant_group_removal(value.forceParticipantGroupRemoval_);
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_column(),
    std::move(value.createColumn_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_column(),
    std::move(value.removeColumn_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group(),
    std::move(value.createColumnGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group(),
    std::move(value.removeColumnGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_add_column_to_group(),
    std::move(value.addColumnToGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_from_group(),
    std::move(value.removeColumnFromGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_create_participant_group(),
    std::move(value.createParticipantGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_group(),
    std::move(value.removeParticipantGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_add_participant_to_group(),
    std::move(value.addParticipantToGroup_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_remove_participant_from_group(),
    std::move(value.removeParticipantFromGroup_));

  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_column_group_access_rule(),
    std::move(value.createColumnGroupAccessRule_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_column_group_access_rule(),
    std::move(value.removeColumnGroupAccessRule_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_create_participant_group_access_rule(),
    std::move(value.createParticipantGroupAccessRule_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_remove_participant_group_access_rule(),
    std::move(value.removeParticipantGroupAccessRule_));
}

AmaQueryResponse Serializer<AmaQueryResponse>::fromProtocolBuffer(proto::AmaQueryResponse&& source) const {
  AmaQueryResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.columns_,
    std::move(*source.mutable_columns()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.columnGroups_,
    std::move(*source.mutable_column_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.columnGroupAccessRules_,
    std::move(*source.mutable_column_group_access_rules()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.participantGroups_,
    std::move(*source.mutable_participant_groups()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.participantGroupAccessRules_,
    std::move(*source.mutable_participant_group_access_rules()));
  return result;
}

void Serializer<AmaQueryResponse>::moveIntoProtocolBuffer(proto::AmaQueryResponse& dest, AmaQueryResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_columns(),
    std::move(value.columns_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_column_groups(),
    std::move(value.columnGroups_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_column_group_access_rules(),
    std::move(value.columnGroupAccessRules_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_groups(),
    std::move(value.participantGroups_));
  Serialization::AssignToRepeatedProtocolBuffer(
    *dest.mutable_participant_group_access_rules(),
    std::move(value.participantGroupAccessRules_));
}

AmaQuery Serializer<AmaQuery>::fromProtocolBuffer(proto::AmaQuery&& source) const {
  return {
    .at_ = source.has_at()
        ? std::optional(Serialization::FromProtocolBuffer(std::move(*source.mutable_at())))
        : std::nullopt,
    .columnFilter_ = std::move(*source.mutable_column_filter()),
    .columnGroupFilter_ = std::move(*source.mutable_column_group_filter()),
    .participantGroupFilter_ = std::move(*source.mutable_participant_group_filter()),
    .userGroupFilter_ = std::move(*source.mutable_user_group_filter()),
    .columnGroupModeFilter_ = std::move(*source.mutable_column_group_mode_filter()),
    .participantGroupModeFilter_ = std::move(*source.mutable_participant_group_mode_filter())
  };
}

void Serializer<AmaQuery>::moveIntoProtocolBuffer(proto::AmaQuery& dest, AmaQuery value) const {
  if (value.at_) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_at(), *value.at_);
  }
  *dest.mutable_column_filter() = std::move(value.columnFilter_);
  *dest.mutable_column_group_filter() = std::move(value.columnGroupFilter_);
  *dest.mutable_participant_group_filter() = std::move(value.participantGroupFilter_);
  *dest.mutable_user_group_filter() = std::move(value.userGroupFilter_);
  *dest.mutable_column_group_mode_filter() = std::move(value.columnGroupModeFilter_);
  *dest.mutable_participant_group_mode_filter() = std::move(value.participantGroupModeFilter_);
}

AmaQRColumn Serializer<AmaQRColumn>::fromProtocolBuffer(proto::AmaQRColumn&& source) const {
  return AmaQRColumn(std::move(*source.mutable_name()));
}

void Serializer<AmaQRColumn>::moveIntoProtocolBuffer(proto::AmaQRColumn& dest, AmaQRColumn value) const {
  *dest.mutable_name() = std::move(value.name_);
}

AmaQRParticipantGroup Serializer<AmaQRParticipantGroup>::fromProtocolBuffer(proto::AmaQRParticipantGroup&& source) const {
  return AmaQRParticipantGroup(std::move(*source.mutable_name()));
}

void Serializer<AmaQRParticipantGroup>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroup& dest, AmaQRParticipantGroup value) const {
  *dest.mutable_name() = std::move(value.name_);
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
  *dest.mutable_name() = std::move(value.name_);
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns_.size()));
  for (auto& x : value.columns_)
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
  *dest.mutable_column_group() = std::move(value.columnGroup_);
  *dest.mutable_access_group() = std::move(value.accessGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

AmaQRParticipantGroupAccessRule Serializer<AmaQRParticipantGroupAccessRule>::fromProtocolBuffer(proto::AmaQRParticipantGroupAccessRule&& source) const {
  return AmaQRParticipantGroupAccessRule(
    std::move(*source.mutable_participant_group()),
    std::move(*source.mutable_user_group()),
    std::move(*source.mutable_mode())
  );
}

void Serializer<AmaQRParticipantGroupAccessRule>::moveIntoProtocolBuffer(proto::AmaQRParticipantGroupAccessRule& dest, AmaQRParticipantGroupAccessRule value) const {
  *dest.mutable_participant_group() = std::move(value.participantGroup_);
  *dest.mutable_user_group() = std::move(value.userGroup_);
  *dest.mutable_mode() = std::move(value.mode_);
}

}
