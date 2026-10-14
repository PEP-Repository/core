#pragma once

#include <pep/accessmanager/Storage.hpp>
#include <pep/accessmanager/UserStorageRecords.hpp>
#include <pep/database/Storage.hpp>

#include <sqlite_orm/sqlite_orm.h>

namespace pep {

namespace detail {

// This function defines the database scheme used.
inline auto CreateAmDb(const std::string& path) {
  using namespace sqlite_orm;

  return make_storage(path,
    make_table("SelectStarPseudonyms",
      make_column("LocalPseudonym",
        &SelectStarPseudonymRecord::localPseudonym),
      make_column("PolymorphicPseudonym",
        &SelectStarPseudonymRecord::polymorphicPseudonym),
      make_column("seqno",
        &SelectStarPseudonymRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_ParticipantGroups",
      &ParticipantGroupRecord::name,
      &ParticipantGroupRecord::timestamp),
    make_table("ParticipantGroups",
      make_column("seqno",
        &ParticipantGroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ParticipantGroupRecord::checksumNonce),
      make_column("timestamp", &ParticipantGroupRecord::timestamp),
      make_column("tombstone", &ParticipantGroupRecord::tombstone),
      make_column("name", &ParticipantGroupRecord::name)),

    make_index("idx_ParticipantGroupParticipants",
      &ParticipantGroupParticipantRecord::localPseudonym,
      &ParticipantGroupParticipantRecord::participantGroup,
      &ParticipantGroupParticipantRecord::timestamp),
    make_table("ParticipantGroupParticipants",
      make_column("seqno",
        &ParticipantGroupParticipantRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ParticipantGroupParticipantRecord::checksumNonce),
      make_column("timestamp", &ParticipantGroupParticipantRecord::timestamp),
      make_column("tombstone", &ParticipantGroupParticipantRecord::tombstone),
      make_column("localPseudonym", &ParticipantGroupParticipantRecord::localPseudonym),
      make_column("participantGroup", &ParticipantGroupParticipantRecord::participantGroup)),

    make_index("idx_ColumnGroups",
      &ColumnGroupRecord::name,
      &ColumnGroupRecord::timestamp),
    make_table("ColumnGroups",
      make_column("seqno",
        &ColumnGroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ColumnGroupRecord::checksumNonce),
      make_column("timestamp", &ColumnGroupRecord::timestamp),
      make_column("tombstone", &ColumnGroupRecord::tombstone),
      make_column("name", &ColumnGroupRecord::name)),

    make_index("idx_ColumnGroupColumns",
      &ColumnGroupColumnRecord::column,
      &ColumnGroupColumnRecord::columnGroup,
      &ColumnGroupColumnRecord::timestamp),
    make_table("ColumnGroupColumns",
      make_column("seqno",
        &ColumnGroupColumnRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ColumnGroupColumnRecord::checksumNonce),
      make_column("timestamp", &ColumnGroupColumnRecord::timestamp),
      make_column("tombstone", &ColumnGroupColumnRecord::tombstone),
      make_column("column", &ColumnGroupColumnRecord::column),
      make_column("columnGroup", &ColumnGroupColumnRecord::columnGroup)),

    make_index("idx_ColumnGroupAccessRules",
      &ColumnGroupAccessRuleRecord::userGroup,
      &ColumnGroupAccessRuleRecord::timestamp,
      &ColumnGroupAccessRuleRecord::columnGroup,
      &ColumnGroupAccessRuleRecord::mode),
    make_table("ColumnGroupAccessRules",
      make_column("seqno",
        &ColumnGroupAccessRuleRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ColumnGroupAccessRuleRecord::checksumNonce),
      make_column("timestamp", &ColumnGroupAccessRuleRecord::timestamp),
      make_column("tombstone", &ColumnGroupAccessRuleRecord::tombstone),
      make_column("columnGroup", &ColumnGroupAccessRuleRecord::columnGroup),
      make_column("accessGroup", &ColumnGroupAccessRuleRecord::userGroup),
      make_column("mode", &ColumnGroupAccessRuleRecord::mode)),

    make_index("idx_GroupAccessRules",
      &ParticipantGroupAccessRuleRecord::userGroup,
      &ParticipantGroupAccessRuleRecord::timestamp,
      &ParticipantGroupAccessRuleRecord::participantGroup,
      &ParticipantGroupAccessRuleRecord::mode),
    make_table("GroupAccessRules",
      make_column("seqno",
        &ParticipantGroupAccessRuleRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ParticipantGroupAccessRuleRecord::checksumNonce),
      make_column("timestamp", &ParticipantGroupAccessRuleRecord::timestamp),
      make_column("tombstone", &ParticipantGroupAccessRuleRecord::tombstone),
      make_column("group", &ParticipantGroupAccessRuleRecord::participantGroup),
      make_column("accessGroup", &ParticipantGroupAccessRuleRecord::userGroup),
      make_column("mode", &ParticipantGroupAccessRuleRecord::mode)),

    make_index("idx_Columns",
      &ColumnRecord::name,
      &ColumnRecord::timestamp),
    make_table("Columns",
      make_column("seqno",
        &ColumnRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &ColumnRecord::checksumNonce),
      make_column("timestamp", &ColumnRecord::timestamp),
      make_column("tombstone", &ColumnRecord::tombstone),
      make_column("name", &ColumnRecord::name)),

    make_unique_index("idx_ColumnNameMappings",
      &ColumnNameMappingRecord::original),
    make_table("ColumnNameMappings",
      make_column("original", &ColumnNameMappingRecord::original),
      make_column("mapped", &ColumnNameMappingRecord::mapped)),

    make_index("idx_UserIds",
      &UserIdRecord::internalUserId,
      &UserIdRecord::identifier,
      &UserIdRecord::timestamp),
    make_table("UserIds",
      make_column("seqno",
        &UserIdRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserIdRecord::checksumNonce),
      make_column("timestamp", &UserIdRecord::timestamp),
      make_column("tombstone", &UserIdRecord::tombstone),
      make_column("internalUserId", &UserIdRecord::internalUserId),
      make_column("identifier", &UserIdRecord::identifier),
      make_column("isPrimaryId", &UserIdRecord::isPrimaryId, default_value(false)),
      make_column("isDisplayId", &UserIdRecord::isDisplayId, default_value(false))),

    make_index("idx_UserGroups",
      &UserGroupRecord::userGroupId,
      &UserGroupRecord::name,
      &UserGroupRecord::timestamp),
    make_table("UserGroups",
      make_column("seqno",
        &UserGroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserGroupRecord::checksumNonce),
      make_column("timestamp", &UserGroupRecord::timestamp),
      make_column("tombstone", &UserGroupRecord::tombstone),
      make_column("userGroupId", &UserGroupRecord::userGroupId),
      make_column("name", &UserGroupRecord::name),
      make_column("maxAuthValiditySeconds", &UserGroupRecord::maxAuthValiditySeconds)),

    make_index("idx_UserGroupUsers",
      &UserGroupUserRecord::internalUserId,
      &UserGroupUserRecord::userGroupId,
      &UserGroupUserRecord::timestamp),
    make_table("UserGroupUsers",
      make_column("seqno",
        &UserGroupUserRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserGroupUserRecord::checksumNonce),
      make_column("timestamp", &UserGroupUserRecord::timestamp),
      make_column("tombstone", &UserGroupUserRecord::tombstone),
      make_column("internalUserId", &UserGroupUserRecord::internalUserId),
      make_column("userGroupId", &UserGroupUserRecord::userGroupId),
      make_column("expirationTimestamp", &UserGroupUserRecord::expirationTimestamp)),

    make_index("idx_StructureMetadata",
      &StructureMetadataRecord::subjectType,
      &StructureMetadataRecord::subject,
      &StructureMetadataRecord::timestamp),
    make_table("StructureMetadata",
      make_column("seqno",
        &StructureMetadataRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &StructureMetadataRecord::checksumNonce),
      make_column("timestamp", &StructureMetadataRecord::timestamp),
      make_column("tombstone", &StructureMetadataRecord::tombstone),
      make_column("subjectType", &StructureMetadataRecord::subjectType),
      make_column("subject", &StructureMetadataRecord::subject),
      make_column("internalSubjectId", &StructureMetadataRecord::internalSubjectId),
      make_column("metadataGroup", &StructureMetadataRecord::metadataGroup),
      make_column("subkey", &StructureMetadataRecord::subkey),
      make_column("value", &StructureMetadataRecord::value))
  );
}

} // End namespace detail

// Can't be a typedef because we need to forward declare it for our pimpl idiom
struct AccessManager::Backend::Storage::Implementor : database::Storage<detail::CreateAmDb> {
  using Storage::Storage;
};

}
