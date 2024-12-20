#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/serialization/IndexListSerializer.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

namespace pep {

IndexedTicket2 Serializer<IndexedTicket2>::fromProtocolBuffer(proto::IndexedTicket2&& source) const {
  std::unordered_map<std::string, IndexList> groups;
  std::unordered_map<std::string, IndexList> columnGroups;
  for (auto& kv : *source.mutable_groups())
    groups[kv.first] = Serialization::FromProtocolBuffer(std::move(kv.second));
  for (auto& kv : *source.mutable_column_groups())
    columnGroups[kv.first] = Serialization::FromProtocolBuffer(std::move(kv.second));
  return IndexedTicket2(
    std::make_shared<SignedTicket2>(
      Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()))),
    std::move(columnGroups),
    std::move(groups));
}

void Serializer<IndexedTicket2>::moveIntoProtocolBuffer(proto::IndexedTicket2& dest, IndexedTicket2 value) const {
  for (auto& kv : value.getParticipantGroupMapping())
    Serialization::MoveIntoProtocolBuffer((*dest.mutable_groups())[kv.first], kv.second);
  for (auto& kv : value.getColumnGroupMapping())
    Serialization::MoveIntoProtocolBuffer((*dest.mutable_column_groups())[kv.first], kv.second);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), *value.getTicket());
}

KeyRequestEntry Serializer<KeyRequestEntry>::fromProtocolBuffer(proto::KeyRequestEntry&& source) const {
  KeyRequestEntry result;
  result.mMetadata = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.mPolymorphEncryptionKey = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorph_encryption_key()));
  result.mKeyBlindMode = Serialization::FromProtocolBuffer(source.blind_mode());
  result.mPseudonymIndex = source.pseudonym_index();
  return result;
}

void Serializer<KeyRequestEntry>::moveIntoProtocolBuffer(proto::KeyRequestEntry& dest, KeyRequestEntry value) const {
  dest.set_blind_mode(Serialization::ToProtocolBuffer(value.mKeyBlindMode));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.mMetadata));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorph_encryption_key(), value.mPolymorphEncryptionKey);
  dest.set_pseudonym_index(value.mPseudonymIndex);
}

EncryptionKeyRequest Serializer<EncryptionKeyRequest>::fromProtocolBuffer(proto::EncryptionKeyRequest&& source) const {
  EncryptionKeyRequest result;
  result.mTicket2 = std::make_shared<SignedTicket2>(Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket2())));

  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));

  return result;
}

void Serializer<EncryptionKeyRequest>::moveIntoProtocolBuffer(proto::EncryptionKeyRequest& dest, EncryptionKeyRequest value) const {
  if (value.mTicket2 == nullptr)
    throw std::runtime_error("mTicket2 should be set");
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket2(), *value.mTicket2);
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
}

EncryptionKeyResponse Serializer<EncryptionKeyResponse>::fromProtocolBuffer(proto::EncryptionKeyResponse&& source) const {
  EncryptionKeyResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mKeys, std::move(*source.mutable_keys()));
  return result;
}

void Serializer<EncryptionKeyResponse>::moveIntoProtocolBuffer(proto::EncryptionKeyResponse& dest, EncryptionKeyResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_keys(), std::move(value.mKeys));
}

ColumnAccessRequest Serializer<ColumnAccessRequest>::fromProtocolBuffer(proto::ColumnAccessRequest&& source) const {
  auto result = ColumnAccessRequest{ source.includeimplicitlygranted(), {} };
  result.requireModes.reserve(static_cast<size_t>(source.requiremodes_size()));
  for (auto mode : source.requiremodes()) {
    result.requireModes.push_back(mode);
  }
  return result;
}

void Serializer<ColumnAccessRequest>::moveIntoProtocolBuffer(proto::ColumnAccessRequest& dest, ColumnAccessRequest value) const {
  dest.set_includeimplicitlygranted(value.includeImplicitlyGranted);
  dest.mutable_requiremodes()->Reserve(static_cast<int>(value.requireModes.size()));
  for (auto& mode : value.requireModes) {
    dest.add_requiremodes(std::move(mode));
  }
}

ColumnAccessResponse Serializer<ColumnAccessResponse>::fromProtocolBuffer(proto::ColumnAccessResponse&& source) const {
  if (source.columngroups_size() != source.columngroupcolumns_size()) {
    std::ostringstream msg;
    msg << "Invalid Column Access specification: " << source.columngroupcolumns_size() << " column set(s) specified for " << source.columngroups_size() << " column group(s)";
    throw std::runtime_error(msg.str());
  }

  ColumnAccessResponse result;
  result.columnGroups.reserve(static_cast<unsigned>(source.columngroups_size()));

  for (int i = 0; i < source.columngroups_size(); ++i) {
    auto& entry = source.columngroups(i);
    auto indices = source.columngroupcolumns(i);

    ColumnAccess::GroupProperties properties;
    properties.modes.reserve(static_cast<unsigned>(entry.modes_size()));
    for (const auto& mode : entry.modes()) {
      properties.modes.push_back(mode);
    }
    properties.columns = Serialization::FromProtocolBuffer(std::move(indices));
    result.columnGroups[entry.name()] = properties;
  }

  result.columns.reserve(static_cast<unsigned>(source.columns_size()));
  for (auto column : source.columns()) {
    result.columns.push_back(std::move(column));
  }

  return result;
}

void Serializer<ColumnAccessResponse>::moveIntoProtocolBuffer(proto::ColumnAccessResponse& dest, ColumnAccessResponse value) const {
  dest.mutable_columngroups()->Reserve(static_cast<int>(value.columnGroups.size()));
  dest.mutable_columngroupcolumns()->Reserve(static_cast<int>(value.columnGroups.size()));

  for (auto& cg : value.columnGroups) {
    proto::ColumnGroupAccess entry;
    entry.set_name(cg.first);
    entry.mutable_modes()->Reserve(static_cast<int>(cg.second.modes.size()));
    for (auto& mode : cg.second.modes) {
      entry.add_modes(std::move(mode));
    }

    dest.add_columngroups()->CopyFrom(entry);
    dest.add_columngroupcolumns()->CopyFrom(Serialization::ToProtocolBuffer(cg.second.columns));
  }

  dest.mutable_columns()->Reserve(static_cast<int>(value.columns.size()));
  for (auto& column : value.columns) {
    dest.add_columns(std::move(column));
  }
}

ParticipantGroupAccessRequest Serializer<ParticipantGroupAccessRequest>::fromProtocolBuffer(proto::ParticipantGroupAccessRequest&& source) const {
  return ParticipantGroupAccessRequest{ source.includeimplicitlygranted() };
}

void Serializer<ParticipantGroupAccessRequest>::moveIntoProtocolBuffer(proto::ParticipantGroupAccessRequest& dest, ParticipantGroupAccessRequest value) const {
  dest.set_includeimplicitlygranted(value.includeImplicitlyGranted);
}

ParticipantGroupAccessResponse Serializer<ParticipantGroupAccessResponse>::fromProtocolBuffer(proto::ParticipantGroupAccessResponse&& source) const {
  ParticipantGroupAccessResponse result;
  result.participantGroups.reserve(static_cast<size_t>(source.participantgroups_size()));
  for (auto& entry : source.participantgroups()) {
    std::vector<std::string> modes;
    modes.reserve(static_cast<size_t>(entry.modes_size()));
    modes.insert(modes.end(), entry.modes().begin(), entry.modes().end());
    result.participantGroups.emplace(entry.name(), std::move(modes));
  }
  return result;
}

void Serializer<ParticipantGroupAccessResponse>::moveIntoProtocolBuffer(proto::ParticipantGroupAccessResponse& dest, ParticipantGroupAccessResponse value) const {
  dest.mutable_participantgroups()->Reserve(static_cast<int>(value.participantGroups.size()));

  for (auto& pg : value.participantGroups) {
    proto::ParticipantGroupAccess entry;
    entry.set_name(pg.first);
    entry.mutable_modes()->Reserve(static_cast<int>(pg.second.size()));
    for (auto& mode : pg.second) {
      entry.add_modes(std::move(mode));
    }
    dest.add_participantgroups()->CopyFrom(entry);
  }
}

void Serializer<ColumnNameMappingRequest>::moveIntoProtocolBuffer(proto::ColumnNameMappingRequest& dest, ColumnNameMappingRequest value) const {
  dest.set_action(Serialization::ToProtocolBuffer(value.action));
  if (value.original.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_original(), *value.original);
  }
  if (value.mapped.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_mapped(), *value.mapped);
  }
}

ColumnNameMappingRequest Serializer<ColumnNameMappingRequest>::fromProtocolBuffer(proto::ColumnNameMappingRequest&& source) const {
  ColumnNameMappingRequest result;
  result.action = Serialization::FromProtocolBuffer(source.action());
  if (source.has_original()) {
    result.original = Serialization::FromProtocolBuffer(std::move(*source.mutable_original()));
  }
  if (source.has_mapped()) {
    result.mapped = Serialization::FromProtocolBuffer(std::move(*source.mutable_mapped()));
  }
  return result;
}

void Serializer<ColumnNameMappingResponse>::moveIntoProtocolBuffer(proto::ColumnNameMappingResponse& dest, ColumnNameMappingResponse value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_mappings(), value.mappings);
}

ColumnNameMappingResponse Serializer<ColumnNameMappingResponse>::fromProtocolBuffer(proto::ColumnNameMappingResponse&& source) const {
  ColumnNameMappingResponse result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mappings,
    std::move(*source.mutable_mappings()));
  return result;
}

}