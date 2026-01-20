// Storage class for the access manager.

#include <pep/accessmanager/Storage.hpp>

#include <pep/accessmanager/UserStorageRecords.hpp>
#include <pep/accessmanager/LegacyAuthserverStorage.hpp>

#include <pep/auth/UserGroup.hpp>
#include <pep/database/Storage.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <iterator>
#include <ranges>
#include <set>

#include <sqlite_orm/sqlite_orm.h>

#include <boost/algorithm/string/join.hpp>
#include <filesystem>
#include <unordered_set>

using std::chrono::milliseconds;

namespace pep {

namespace {

const std::vector<std::string> emptyVector{}; // Used as a default for optional vectors.
using namespace sqlite_orm;

const std::string LOG_TAG("AccessManager::Backend::Storage");

using id_pair_set = std::unordered_set<std::pair<int64_t, int64_t>,
  boost::hash<std::pair<int64_t, int64_t>>>;

std::optional<std::chrono::seconds> to_optional_seconds(std::optional<uint64_t> val) {
  if (val) {
    return std::chrono::seconds(*val);
  }
  return std::nullopt;
}

std::optional<uint64_t> to_optional_uint64(std::optional<std::chrono::seconds> val) {
  if (val) {
    return val->count();
  }
  return std::nullopt;
}

// This function defines the database scheme used.
auto am_create_db(const std::string& path) {
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
      make_column("userGroupId", &UserGroupUserRecord::userGroupId)),

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

}

// Can't be a typedef because we need to forward declare it for our pimpl idiom
struct AccessManager::Backend::Storage::Implementor : database::Storage<am_create_db> {
  using Storage::Storage;
};

void AccessManager::Backend::Storage::ensureInitialized() {
  mImplementor->syncSchema();

  if (mImplementor->raw.count<ColumnGroupRecord>(limit(1)) != 0)
    return;

  LOG(LOG_TAG, warning) << "Database seems uninitialized.  Initializing ...";

  // Column groups
  mImplementor->raw.insert(ColumnGroupRecord("*"));
  mImplementor->raw.insert(ColumnGroupRecord("ShortPseudonyms"));
  mImplementor->raw.insert(ColumnGroupRecord("CastorShortPseudonyms"));
  mImplementor->raw.insert(ColumnGroupRecord("WatchData"));
  mImplementor->raw.insert(ColumnGroupRecord("Castor"));
  mImplementor->raw.insert(ColumnGroupRecord("Device"));
  mImplementor->raw.insert(ColumnGroupRecord("VisitAssessors"));

  // Column with identically-named single column column-group.
  for (const auto& c : {
      "Canary",
      "IsTestParticipant",
      "ParticipantInfo",
      "ParticipantIdentifier",
      "StudyContexts"}) {
    mImplementor->raw.insert(ColumnGroupRecord(c));
    mImplementor->raw.insert(ColumnRecord(c));
    mImplementor->raw.insert(ColumnGroupColumnRecord(c, c));
  }

  // Column group access
  // ///////////////////////////////////////////////////////////////////////////

  // Registration sever
  const auto registrationServer = *ServerTraits::RegistrationServer().enrollmentSubject(true);
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", registrationServer, "access"));
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", registrationServer, "enumerate"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("ShortPseudonyms", registrationServer, "read"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("ShortPseudonyms", registrationServer, "write"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("ParticipantIdentifier", registrationServer, "read"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("ParticipantIdentifier", registrationServer, "write"));

  // "Pull castor" server
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", "PullCastor", "access"));
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", "PullCastor", "enumerate"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("CastorShortPseudonyms", "PullCastor", "read"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("Castor", "PullCastor", "read"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("Castor", "PullCastor", "write"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("Device", "PullCastor", "read"));

  // Research assessor
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::ResearchAssessor, "access"));
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::ResearchAssessor, "enumerate"));
  for (const auto& cg : {
      "ShortPseudonyms", "WatchData", "Device", "ParticipantIdentifier", "ParticipantInfo", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->raw.insert(ColumnGroupAccessRuleRecord(cg, UserGroup::ResearchAssessor, "read"));
  for (const auto& cg : {"Device", "ParticipantInfo", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->raw.insert(ColumnGroupAccessRuleRecord(cg, UserGroup::ResearchAssessor, "write"));

  // Monitor
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::Monitor, "access"));
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::Monitor, "enumerate"));
  for (const auto& cg : {
      "ShortPseudonyms", "Device", "ParticipantIdentifier", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->raw.insert(ColumnGroupAccessRuleRecord(cg, UserGroup::Monitor, "read"));

  // Data administrator
  // DA has unchecked access to all participant groups: don't grant explicit privileges. See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1923#note_22224
  for (const auto& cg : {"ShortPseudonyms", "WatchData",
      "ParticipantIdentifier", "Device", "Castor", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->raw.insert(ColumnGroupAccessRuleRecord(cg, UserGroup::DataAdministrator, "read"));

  // Watchdog
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::Watchdog, "access")); // TODO reduce
  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord("*", UserGroup::Watchdog, "enumerate")); // TODO reduce
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("Canary", UserGroup::Watchdog, "read"));
  mImplementor->raw.insert(ColumnGroupAccessRuleRecord("Canary", UserGroup::Watchdog, "write"));


#if defined(ENABLE_OAUTH_TEST_USERS) && defined(AUTO_POPULATE_USER_DB)
  // For testing purposes, we want some users which can request long-lived tokens, and some that can't.
  // I chose data admin as the role that can get long-lived tokens, but it could have been any user/group entry
  createUserGroup(UserGroup(UserGroup::ResearchAssessor, std::nullopt));
  createUserGroup(UserGroup(UserGroup::Monitor, std::nullopt));
  createUserGroup(UserGroup(UserGroup::DataAdministrator, std::chrono::days{1}));
  createUserGroup(UserGroup(UserGroup::AccessAdministrator, std::nullopt));
  createUserGroup(UserGroup(UserGroup::SystemAdministrator, std::nullopt));

  auto assessorId = createUser("assessor@master.pep.cs.ru.nl");
  auto monitorId = createUser("monitor@master.pep.cs.ru.nl");
  auto dataadminId = createUser("dataadmin@master.pep.cs.ru.nl");
  auto accessadminId = createUser("accessadmin@master.pep.cs.ru.nl");
  auto systemadminId = createUser("systemadmin@master.pep.cs.ru.nl");
  auto multihatId = createUser("multihat@master.pep.cs.ru.nl");

  addUserToGroup(assessorId, UserGroup::ResearchAssessor);
  addUserToGroup(monitorId, UserGroup::Monitor);
  addUserToGroup(dataadminId, UserGroup::DataAdministrator);
  addUserToGroup(accessadminId, UserGroup::AccessAdministrator);
  addUserToGroup(systemadminId, UserGroup::SystemAdministrator);

  addUserToGroup(multihatId, UserGroup::ResearchAssessor);
  addUserToGroup(multihatId, UserGroup::Monitor);
  addUserToGroup(multihatId, UserGroup::DataAdministrator);
  addUserToGroup(multihatId, UserGroup::AccessAdministrator);
  addUserToGroup(multihatId, UserGroup::SystemAdministrator);

#endif //ENABLE_OAUTH_TEST_USERS


  LOG(LOG_TAG, info) << "  ... done";
}

std::set<std::string> AccessManager::Backend::Storage::ensureSynced() {
  LOG(LOG_TAG, info) << "Checking whether to create/remove columns ...";
  std::set<std::string> allColumns;
  for (auto& col : this->getColumns(TimeNow())) {
    allColumns.insert(col.name);
  }
  auto ensureColumnExists = [implementor = mImplementor, &allColumns](const std::string& column) {
    if (allColumns.count(column) == 0) {
      LOG(LOG_TAG, warning) << "  adding column " << column;
      allColumns.insert(column);
      implementor->raw.insert(ColumnRecord(column));
    }
  };

  // Create a column for each visit's administering assessor
  std::set<std::string> visitAssessorColumns;
  for (const auto& context : mGlobalConf->getStudyContexts().getItems()) {
    for (const auto& column : mGlobalConf->getVisitAssessorColumns(context)) {
      ensureColumnExists(column);
      visitAssessorColumns.insert(column);
    }
  }
  syncColumnGroupContents("VisitAssessors", visitAssessorColumns);

  // Create a column for each short pseudonym
  std::set<std::string> spColumns, castorSpColumns;
  for (const auto& sp : mGlobalConf->getShortPseudonyms()) {
    auto column = sp.getColumn().getFullName();
    spColumns.insert(column);
    if (sp.getCastor()) {
      castorSpColumns.insert(column);
    }
    ensureColumnExists(column);
  }
  syncColumnGroupContents("ShortPseudonyms", spColumns);
  syncColumnGroupContents("CastorShortPseudonyms", castorSpColumns);

  // Create a column for each device (history) definition
  std::set<std::string> deviceColumns;
  for (const auto& device : mGlobalConf->getDevices()) {
    auto& column = device.columnName;
    deviceColumns.insert(column);
    ensureColumnExists(column);
  }
  syncColumnGroupContents("Device", deviceColumns);

  // Ensure the "*" column group is in synch.
  syncColumnGroupContents("*", allColumns);

  return allColumns;
}

void AccessManager::Backend::Storage::checkConfig(const std::set<std::string>& allColumns) const {
  for (const auto &colSpec : mGlobalConf->getColumnSpecifications()) {
    const std::string &name = colSpec.getColumn();
    if (allColumns.find(name) == allColumns.end()) {
      // Just warn, the column may be created later
      LOG(LOG_TAG, warning) << "Column " << Logging::Escape(name) << " mentioned in column_specifications does not exist";
    }
    // Associated short pseudonym column is already checked in GlobalConfiguration::GlobalConfiguration,
    // and was created above
  }
}

void AccessManager::Backend::Storage::ensureUpToDate() {
  LOG(LOG_TAG, info) << "Checking whether to remove participant-group-access-rules ...";
  // Remove explicit PGARs for Data Administrator: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1923#note_22224
  auto pgars = getParticipantGroupAccessRules(TimeNow(), {.userGroups = std::vector<std::string>{UserGroup::DataAdministrator}});
  for (const auto& pgar : pgars) {
    LOG(LOG_TAG, info) << "Removing " << Logging::Escape(pgar.mode) << " access to " << Logging::Escape(pgar.participantGroup) << " participant-group for role " << Logging::Escape(pgar.userGroup);
    this->removeParticipantGroupAccessRule(pgar.participantGroup, pgar.userGroup, pgar.mode);
  }

  /* The accessmanager used to use a the protobuf serialization to store and lookup Local Pseudonyms
   * Protobuf serialization is not guaranteed to be stable, so this could lead to problems if the serialization changes.
   * Therefore, we now use CurvePoint::packString. This method updates existing entries from the old serialization to the new.
   * See issue #1212
   */
  LOG(LOG_TAG, info) << "Checking whether the serialization of local pseudonyms is up to date";
  auto selectStarPseudonymRecordCountTotal = mImplementor->raw.count<SelectStarPseudonymRecord>();
  auto selectStarPseudonymRecordCountOldFormat = mImplementor->raw.count<SelectStarPseudonymRecord>(where(length(&SelectStarPseudonymRecord::localPseudonym) > CurvePoint::PACKEDBYTES &&
      length(&SelectStarPseudonymRecord::polymorphicPseudonym) > ElgamalEncryption::PACKEDBYTES));
  auto participantGroupParticipantRecordCountTotal = mImplementor->raw.count<ParticipantGroupParticipantRecord>();
  auto participantGroupParticipantRecordCountOldFormat = mImplementor->raw.count<ParticipantGroupParticipantRecord>(where(length(&ParticipantGroupParticipantRecord::localPseudonym) > CurvePoint::PACKEDBYTES));

  if (selectStarPseudonymRecordCountOldFormat == 0 && participantGroupParticipantRecordCountOldFormat == 0) {
    LOG(LOG_TAG, info) << "everything up to date";
  }
  else if (selectStarPseudonymRecordCountTotal != selectStarPseudonymRecordCountOldFormat) {
    throw std::runtime_error("Some selectStarPseudonymRecords appear to be updated, but some are in the old format. This should not happen! Either all are updated, or all still need to be updated. "
      + std::to_string(selectStarPseudonymRecordCountOldFormat) + " records out of total of " + std::to_string(selectStarPseudonymRecordCountTotal) + " have the old format.");
  }
  else if (participantGroupParticipantRecordCountTotal != participantGroupParticipantRecordCountOldFormat) {
    throw std::runtime_error("Some participantGroupParticipantRecords appear to be updated, but some are in the old format. This should not happen! Either all are updated, or all still need to be updated."
      + std::to_string(participantGroupParticipantRecordCountOldFormat) + " records out of total of " + std::to_string(participantGroupParticipantRecordCountTotal) + " have the old format");
  }
  else {
    std::filesystem::path backupDirectory = this->mStoragePath.parent_path();
    std::filesystem::create_directories(backupDirectory);
    std::filesystem::path backupPath = backupDirectory / (this->mStoragePath.stem().string() + "_before_lp_and_pp_reserialization" + this->mStoragePath.extension().string());
    if (std::filesystem::exists(backupPath)) {
      throw std::runtime_error("LP and PP format was not up to date, so an upgrade was attempted. But the backup file "
        + backupPath.string() + " already exists. An upgrade was apparently already attempted, but failed. Manual correction is required.");
    }
    std::filesystem::copy_file(this->mStoragePath, backupPath);
    LOG(LOG_TAG, info) << "Backed up storage to " << backupPath << ". Backup is " << std::filesystem::file_size(backupPath) << " bytes.";
    auto transactionGuard = mImplementor->raw.transaction_guard();
    for (auto record : mImplementor->raw.iterate<SelectStarPseudonymRecord>()) {
      CurvePoint localPseudonymAsPoint = Serialization::FromString<CurvePoint>(SpanToString(record.localPseudonym));
      record.localPseudonym = RangeToVector(localPseudonymAsPoint.pack());
      ElgamalEncryption polymorphicPseudonymAsElgamalEncryption = Serialization::FromString<ElgamalEncryption>(SpanToString(record.polymorphicPseudonym));
      record.polymorphicPseudonym = RangeToVector(polymorphicPseudonymAsElgamalEncryption.pack());
      mImplementor->raw.update(record);
    }

    for (auto record : mImplementor->raw.iterate<ParticipantGroupParticipantRecord>()) {
      CurvePoint localPseudonymAsPoint = Serialization::FromString<CurvePoint>(SpanToString(record.localPseudonym));
      record.localPseudonym = RangeToVector(localPseudonymAsPoint.pack());
      mImplementor->raw.update(record);
    }
    transactionGuard.commit();
    LOG(LOG_TAG, info) << "all records have been updated";
  }

  //DisplayIds and PrimaryIds where introduced at the same time. So if there are primaryIds already in the DB, we can also assume that the upgrade already happened before.
  //Furthermore, because we check that there are no primaryIds, in the auto-assignment we don't have to worry about whether identifiers are primaryIds or not.
  if (mImplementor->raw.count(&UserIdRecord::seqno, where(c(&UserIdRecord::isDisplayId) == 1 || c(&UserIdRecord::isPrimaryId) == 1), limit(1)) == 0) {
    LOG(LOG_TAG, info) << "There are no displayIds in the database yet. Auto-assigning...";
    size_t countAssigned = 0;
    size_t countUnassigned = 0;
    auto displayIdTransactionGuard = mImplementor->raw.transaction_guard();
    for (auto userId : mImplementor->getCurrentRecords(true, &UserIdRecord::internalUserId)) {
      auto firstIdentifier = RangeToOptional(
        mImplementor->raw.select(&UserIdRecord::identifier,
        where(c(&UserIdRecord::internalUserId) == userId),
        order_by(&UserIdRecord::seqno).asc(),
        limit(1)));
      if (firstIdentifier) {
        if (mImplementor->currentRecordExists<UserIdRecord>(c(&UserIdRecord::internalUserId) == userId && c(&UserIdRecord::identifier) == *firstIdentifier)) {
          mImplementor->raw.insert(UserIdRecord(userId, *firstIdentifier, UserIdFlags::isDisplayId));
          countAssigned++;
        }
        else if (mImplementor->currentRecordExists<UserIdRecord>(c(&UserIdRecord::internalUserId) == userId)) {
          countUnassigned++;
        }
      }
    }
    displayIdTransactionGuard.commit();
    LOG(LOG_TAG, info) << "A displayId has been assigned to " << countAssigned << " records.";
    if (countUnassigned > 0) {
      LOG(LOG_TAG, warning) << "No displayId could be automatically assigned to " << countUnassigned << " records";
    }
  }
}

void AccessManager::Backend::Storage::removeOrphanedRecords() {
  const Timestamp now = TimeNow();

  auto cgars = getColumnGroupAccessRules(now);
  for (auto& cgar : cgars) {
    if (hasColumnGroup(cgar.columnGroup) == false) {
      LOG(LOG_TAG, warning) << "Removing " << Logging::Escape(cgar.mode) << " access to " << Logging::Escape(cgar.columnGroup) << " column-group for role " << Logging::Escape(cgar.userGroup)<< ", as the column-group is removed.";
      removeColumnGroupAccessRule(cgar.columnGroup, cgar.userGroup, cgar.mode);
    }
  }
  auto pgars = getParticipantGroupAccessRules(now);
  for (auto& pgar : pgars) {
    if (hasParticipantGroup(pgar.participantGroup) == false) {
      LOG(LOG_TAG, warning) << "Removing " << Logging::Escape(pgar.mode) << " access to " << Logging::Escape(pgar.participantGroup) << " participant-group for role " << Logging::Escape(pgar.userGroup) << ", as the participant-group is removed.";
      removeParticipantGroupAccessRule(pgar.participantGroup, pgar.userGroup, pgar.mode);
    }
  }
  auto cgcs = getColumnGroupColumns(now);
  for (auto& cgc : cgcs) {
    if (hasColumnGroup(cgc.columnGroup) == false) {
      LOG(LOG_TAG, warning) << "Removing column-group membership of " << Logging::Escape(cgc.column) << " to " << Logging::Escape(cgc.columnGroup) << ", as the column-group is removed.";
      removeColumnFromGroup(cgc.column, cgc.columnGroup);
    }
    else if (hasColumn(cgc.column) == false) {
      LOG(LOG_TAG, warning) << "Removing column-group membership of " << Logging::Escape(cgc.column) << " to " << Logging::Escape(cgc.columnGroup) << ", as the column is removed.";
      removeColumnFromGroup(cgc.column, cgc.columnGroup);
    }
  }
  auto pgps = getParticipantGroupParticipants(now);
  for(auto & pgp : pgps) {
    if (hasParticipantGroup(pgp.participantGroup) == false) {
      removeParticipantFromGroup(pgp.getLocalPseudonym(), pgp.participantGroup);
    }
  }
}

void AccessManager::Backend::Storage::syncColumnGroupContents(const std::string& columnGroup, const std::set<std::string>& requiredColumns) {
  using namespace std::ranges;

  auto groupColumns = RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ColumnGroupColumnRecord::columnGroup) == columnGroup,
      &ColumnGroupColumnRecord::column)
  );

  std::set<std::string> missingColumns;
  set_difference(requiredColumns, groupColumns, std::inserter(missingColumns, missingColumns.begin()));
  std::set<std::string> strayColumns;
  set_difference(groupColumns, requiredColumns, std::inserter(strayColumns, strayColumns.begin()));
  for (auto& column : strayColumns) {
    LOG(LOG_TAG, warning) << "  removing column "
      << column << " from column-group " << columnGroup;
    mImplementor->raw.insert(ColumnGroupColumnRecord(column, columnGroup, true));
  }
  for (auto& column : missingColumns) {
    LOG(LOG_TAG, warning) << "  adding column " << column << " to column-group " << columnGroup;
    mImplementor->raw.insert(ColumnGroupColumnRecord(column, columnGroup));
  }
}

AccessManager::Backend::Storage::Storage( const std::filesystem::path& path, std::shared_ptr<GlobalConfiguration> globalConf) {
  mStoragePath = path;
  mImplementor = std::make_shared<Implementor>(path.string());
  mGlobalConf = globalConf;

  ensureInitialized();
  auto allColumns = ensureSynced();
  checkConfig(allColumns);
  ensureUpToDate();

  // TODO add some basic inexpensive sanity checks on start-up, including
  //   - are there tombstones for non-existent records
  //   - are there multiple (tombstone) records
  //   - do the columns/column groups/groups mentioned in the rule exist
  //   - are all times in the past

  // Cache select(*) pseudonym list
  LOG(LOG_TAG, info) << "Caching SELECT(*) pseudonym list ...";
  for (const auto& record : mImplementor->raw.iterate<SelectStarPseudonymRecord>())
    mLpToPpMap.emplace(
      record.getLocalPseudonym(),
      record.getPolymorphicPseudonym()
    );

  removeOrphanedRecords();
  LOG(LOG_TAG, info) << "Ready to accept requests!";
}

namespace { // TODO: move together with other anonymously-scoped code (at top of source)

template<typename T, int version = 0>
void ComputeChecksumImpl(std::shared_ptr<AccessManager::Backend::Storage::Implementor> storageImplementor,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint) {
  checkpoint = 1;
  checksum = 0;
  if (!maxCheckpoint)
    maxCheckpoint = std::numeric_limits<int64_t>::max();

  for (const auto& record : storageImplementor->raw.iterate<T>(
      where(c(&T::seqno) < static_cast<int64_t>(*maxCheckpoint) - 1))) {
    checkpoint = std::max(checkpoint, static_cast<uint64_t>(record.seqno + 2));
    if constexpr (version == 0) {
      checksum ^= record.checksum();
    }
    else {
      checksum ^= record.checksum(version);
    }
  }
}

//TODO: this checksum is only useful to check the migration for #1642. When that has succeeded, this checksum can be removed in a following release.
static void ComputeLegacyUserGroupUserChecksumImpl(std::shared_ptr<AccessManager::Backend::Storage::Implementor> storageImplementor, std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint)  {
  checkpoint = 1;
  checksum = 0;
  if (!maxCheckpoint)
    maxCheckpoint = std::numeric_limits<int64_t>::max();

  for (auto record : storageImplementor->raw.iterate<UserGroupUserRecord>(
      where(c(&UserGroupUserRecord::seqno) < static_cast<int64_t>(*maxCheckpoint) - 1))) {
    LegacyUserGroupUserRecord legacyRecord(record);

    bool found = false;
    for (auto &record : storageImplementor->raw.iterate<pep::UserGroupRecord>(
             where(c(&pep::UserGroupRecord::userGroupId) == record.userGroupId
               && c(&pep::UserGroupRecord::timestamp) <= record.timestamp),
             order_by(&pep::UserGroupRecord::seqno).desc(), limit(1))) {
      legacyRecord.group = record.name;
      found = true;
    }
    if(!found)
      throw Error("Could not find user group");

    checkpoint = std::max(checkpoint, static_cast<uint64_t>(legacyRecord.seqno + 2));
    checksum ^= legacyRecord.checksum();
  }
}

// We used to store localPseudonyms and polymorphicPseudonyms as protobufs.
// In order to make sure the conversion went right, we want to make sure there are no checksum chain errors.
// So we add v2 checksums that use the current representation, and convert the local- and polymorphic pseudonyms to the old format for the
// existing checksum. The old version of the checksum can be removed in a later release
const std::unordered_map<std::string, decltype(&ComputeChecksumImpl<SelectStarPseudonymRecord>)>
    computeChecksumImpls {
  {"select-start-pseud", &ComputeChecksumImpl<SelectStarPseudonymRecord, 1>},
  {"select-start-pseud-v2", &ComputeChecksumImpl<SelectStarPseudonymRecord, 2>},
  {"participant-groups", &ComputeChecksumImpl<ParticipantGroupRecord>},
  {"participant-group-participants", &ComputeChecksumImpl<ParticipantGroupParticipantRecord, 1>},
  {"participant-group-participants-v2", &ComputeChecksumImpl<ParticipantGroupParticipantRecord, 2>},
  {"column-groups", &ComputeChecksumImpl<ColumnGroupRecord>},
  {"columns", &ComputeChecksumImpl<ColumnRecord>},
  {"column-group-columns", &ComputeChecksumImpl<ColumnGroupColumnRecord>},
  {"column-group-accessrule", &ComputeChecksumImpl<ColumnGroupAccessRuleRecord>},
  {"group-accessrule", &ComputeChecksumImpl<ParticipantGroupAccessRuleRecord>},
  {"user-ids", &ComputeChecksumImpl<UserIdRecord>},
  {"user-groups", &ComputeChecksumImpl<UserGroupRecord>},
  {"user-group-users", &ComputeChecksumImpl<UserGroupUserRecord>},
  {"user-group-users-legacy", ComputeLegacyUserGroupUserChecksumImpl},
  {"structure-metadata", &ComputeChecksumImpl<StructureMetadataRecord>},
};

}

std::vector<std::string> AccessManager::Backend::Storage::getChecksumChainNames() {
  std::vector<std::string> ret;
  ret.reserve(computeChecksumImpls.size());
  for (const auto& pair : computeChecksumImpls) {
    ret.push_back(pair.first);
  }
  return ret;
}

void AccessManager::Backend::Storage::computeChecksum(const std::string& chain,
      std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
      uint64_t& checkpoint) {
  if (computeChecksumImpls.count(chain) == 0)
    throw Error("No such checksum chain");
  computeChecksumImpls.at(chain)(mImplementor, maxCheckpoint, checksum, checkpoint);
}

std::vector<PolymorphicPseudonym> AccessManager::Backend::Storage::getPPs() {
  return RangeToVector(std::views::values(mLpToPpMap));
}

std::unordered_map<PolymorphicPseudonym, std::unordered_set<std::string> /*participant groups*/> AccessManager::Backend::Storage::getPPs(const std::vector<std::string>& participantGroups) {
  using namespace std::ranges;

  std::unordered_map<PolymorphicPseudonym, std::unordered_set<std::string> /*participant groups*/> ppsAndGroups;

  // Insert all participants for "*" if it was requested
  if (find(participantGroups, "*") != participantGroups.end()) {
    for (const auto& pp : views::values(mLpToPpMap)) {
      ppsAndGroups[pp].insert("*");
    }
  }

  // Handle requested participantGroups
  {
    // Retrieve participant LPs with groups
    auto lpsAndGroups = mImplementor->getCurrentRecords(
      in(&ParticipantGroupParticipantRecord::participantGroup, participantGroups),
      &ParticipantGroupParticipantRecord::localPseudonym,
      &ParticipantGroupParticipantRecord::participantGroup);
    // Map LPs to PPs
    for (auto [localPseudonymPack, participantGroup] : lpsAndGroups) {
      ppsAndGroups[mLpToPpMap.at(LocalPseudonym::FromPacked(SpanToString(localPseudonymPack)))].insert(std::move(participantGroup));
    }
  }
  return ppsAndGroups;
}

bool AccessManager::Backend::Storage::hasLocalPseudonym(const LocalPseudonym& localPseudonym) {
  return mLpToPpMap.contains(localPseudonym);
}

void AccessManager::Backend::Storage::storeLocalPseudonymAndPP(
  const LocalPseudonym& localPseudonym,
  const PolymorphicPseudonym& polymorphicPseudonym) {
  auto rerandPolymorphicPseudonym = polymorphicPseudonym.rerandomize();
  mLpToPpMap.emplace(localPseudonym, rerandPolymorphicPseudonym);
  mImplementor->raw.insert(SelectStarPseudonymRecord(localPseudonym, rerandPolymorphicPseudonym));
}



/* Core operations on ParticipantGroups */

bool AccessManager::Backend::Storage::hasParticipantGroup(const std::string& name) {
  if (name == "*") { return true; }

  return mImplementor->currentRecordExists<ParticipantGroupRecord>(
    c(&ParticipantGroupRecord::name) == name);
}

std::set<ParticipantGroup> AccessManager::Backend::Storage::getParticipantGroups(const Timestamp& timestamp, const ParticipantGroupFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ParticipantGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp) &&
      (!filter.participantGroups.has_value()
        || in(&ParticipantGroupRecord::name, filter.participantGroups.value_or(emptyVector))),
      &ParticipantGroupRecord::name)
    | views::transform([](std::string name) {
      return ParticipantGroup(std::move(name));
    })
  );
}

void AccessManager::Backend::Storage::createParticipantGroup(const std::string& name) {
  if (hasParticipantGroup(name)) {
    std::ostringstream msg;
    msg << "Participant-group " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->raw.insert(ParticipantGroupRecord(name));
}

void AccessManager::Backend::Storage::removeParticipantGroup(const std::string& name, const bool force) {
  if (!hasParticipantGroup(name)) {
    std::ostringstream msg;
    msg << "Participant-group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }

  auto guard = mImplementor->raw.transaction_guard();

  const Timestamp now = TimeNow();

  auto associatedLocalPseudonyms = getParticipantGroupParticipants(now, {.participantGroups = std::vector<std::string>{name}});
  auto associatedAccessRules = getParticipantGroupAccessRules(now, {.participantGroups = std::vector<std::string>{name}});

  if (force) {
    // remove all associated connections to this columngroup
    // ParticipantGroupParticipant (pgp)
    for (auto& pgp : associatedLocalPseudonyms) {
      this->removeParticipantFromGroup(pgp.getLocalPseudonym(), name);
    }
    for (auto& pgar : associatedAccessRules) {
      this->removeParticipantGroupAccessRule(pgar.participantGroup, pgar.userGroup, pgar.mode);
    }
  }
  else if (associatedLocalPseudonyms.size() > 0 || associatedAccessRules.size() > 0) {
    // There where associated participants and or access rules, but force was not given as a parameter. Warn the user.
    std::ostringstream msg;
    msg << "Removing participant-group \"" << name << "\" failed due to\n";
    if (associatedLocalPseudonyms.size() > 0) {
      msg << associatedLocalPseudonyms.size() << " participants found in group.\n";
    }
    if (associatedAccessRules.size() > 0) {
      msg << "found associated column-group-access-rules:\n";
      for (auto& pgar : associatedAccessRules) {
        msg << pgar.mode << " access for usergroup " << pgar.userGroup << "\n";
      }
    }
    msg << "If you still want to remove participant-group \"" << name << "\" and all associated data, consider using the --force flag.";
    throw Error(msg.str());
  }

  // Remove metadata
  for (StructureMetadataKey& key : getStructureMetadataKeys(now, StructureMetadataType::ParticipantGroup, name)) {
    removeStructureMetadata(StructureMetadataType::ParticipantGroup, name, std::move(key));
  }

  // Tombstone participant group
  mImplementor->raw.insert(ParticipantGroupRecord(name, true));

  guard.commit();
}

/* Core operations on ParticipantGroupParticipants */
bool AccessManager::Backend::Storage::hasParticipantInGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup) {
  return mImplementor->currentRecordExists<ParticipantGroupParticipantRecord>(
    c(&ParticipantGroupParticipantRecord::localPseudonym) == RangeToVector(localPseudonym.pack())
    && c(&ParticipantGroupParticipantRecord::participantGroup) == participantGroup);
}

std::set<ParticipantGroupParticipant> AccessManager::Backend::Storage::getParticipantGroupParticipants(
  const Timestamp& timestamp, const ParticipantGroupParticipantFilter& filter) const {
  using namespace std::ranges;

  std::vector<std::vector<char>> serializedLocalPseudonyms;
  // create a serialized vector of the LocalPseudonyms for look up.
  if (filter.localPseudonyms.has_value()) {
    serializedLocalPseudonyms = RangeToVector(*filter.localPseudonyms
      | views::transform([](const LocalPseudonym& localPseudonym) {
        return RangeToVector(localPseudonym.pack());
      }));
  }

  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ParticipantGroupParticipantRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.participantGroups.has_value()
        || in(&ParticipantGroupParticipantRecord::participantGroup, filter.participantGroups.value_or(emptyVector)))
      && (!filter.localPseudonyms.has_value()
        || in(&ParticipantGroupParticipantRecord::localPseudonym, serializedLocalPseudonyms)),
      &ParticipantGroupParticipantRecord::participantGroup,
      &ParticipantGroupParticipantRecord::localPseudonym)
    | views::transform([](auto tuple) {
      auto& [participantGroup, localPseudonym] = tuple;
      return ParticipantGroupParticipant(std::move(participantGroup), std::move(localPseudonym));
    })
  );
}

void AccessManager::Backend::Storage::addParticipantToGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup) {
  std::ostringstream msg;
  if (hasParticipantInGroup(localPseudonym, participantGroup)) {
    // Reporting this error to the data manager allows him to link polymorphic pseudonyms of the same participant. However, he will have this ability anyhow by adding a participant and checking if anything changed (e.g. by performing a list on the participant group)
    msg << "Participant is already in participant-group: " << Logging::Escape(participantGroup);
    throw Error(msg.str());
  }
  if (!hasParticipantGroup(participantGroup)) {
    msg << "No such participant-group: " << Logging::Escape(participantGroup);
    throw Error(msg.str());
  }

  if (!hasLocalPseudonym(localPseudonym)) {
    throw Error("No such participant known");
  }

  mImplementor->raw.insert(ParticipantGroupParticipantRecord(localPseudonym, participantGroup));
}

void AccessManager::Backend::Storage::removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup) {
  if (!hasParticipantInGroup(localPseudonym, participantGroup)) {
    // Reporting this error to the data manager allows him to link polymorphic pseudonyms of the same participant. However, he will have this ability anyhow by removing a participant and checking if anything changed (e.g. by performing a list on the participant group)
    std::ostringstream msg;
    msg << "This participant is not part of participant-group " << Logging::Escape(participantGroup);
    throw Error(msg.str());
  }

  mImplementor->raw.insert(ParticipantGroupParticipantRecord(localPseudonym, participantGroup, true));
}


/* Core operations on ParticipantGroup Access Rules */
bool AccessManager::Backend::Storage::hasParticipantGroupAccessRule(
    const std::string& participantGroup,
    const std::string& userGroup,
    const std::string& mode) {
  return mImplementor->currentRecordExists<ParticipantGroupAccessRuleRecord>(
    c(&ParticipantGroupAccessRuleRecord::participantGroup) == participantGroup
    && c(&ParticipantGroupAccessRuleRecord::userGroup) == userGroup
    && c(&ParticipantGroupAccessRuleRecord::mode) == mode);
}

std::set<ParticipantGroupAccessRule> AccessManager::Backend::Storage::getParticipantGroupAccessRules(
  const Timestamp& timestamp, const ParticipantGroupAccessRuleFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ParticipantGroupAccessRuleRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.participantGroups.has_value()
        || in(&ParticipantGroupAccessRuleRecord::participantGroup, filter.participantGroups.value_or(emptyVector)))
      && (!filter.userGroups.has_value()
        || in(&ParticipantGroupAccessRuleRecord::userGroup, filter.userGroups.value_or(emptyVector)))
      && (!filter.modes.has_value()
        || in(&ParticipantGroupAccessRuleRecord::mode, filter.modes.value_or(emptyVector))),
      &ParticipantGroupAccessRuleRecord::participantGroup,
      &ParticipantGroupAccessRuleRecord::userGroup,
      &ParticipantGroupAccessRuleRecord::mode)
    | views::transform([](auto tuple) {
      auto& [participantGroup, userGroup, mode] = tuple;
      return ParticipantGroupAccessRule(std::move(participantGroup), std::move(userGroup), std::move(mode));
    })
  );
}

void AccessManager::Backend::Storage::createParticipantGroupAccessRule(
    const std::string& participantGroup, const std::string& userGroup, const std::string& mode) {
  std::ostringstream msg;

  if (!hasParticipantGroup(participantGroup)) {
        msg << "No such participant-group " << Logging::Escape(participantGroup);
        throw Error(msg.str());
  }
  if (userGroup == UserGroup::DataAdministrator) {
        // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1923#note_22224
        msg << "Cannot create explicit participant-group-access-rules for "
            << Logging::Escape(userGroup)
            << " because all participant-groups are implicitly accessible";
        throw Error(msg.str());
  }
  if (mode != "enumerate" && mode != "access") {
        msg << "No such mode " << Logging::Escape(mode);
        throw Error(msg.str());
  }
  if (hasParticipantGroupAccessRule(participantGroup, userGroup, mode)) {
        msg << "This participant-group-access-rule already exists: ("
            << Logging::Escape(participantGroup) << ", "
            << Logging::Escape(userGroup) << ", "
            << Logging::Escape(mode) << ")";
        throw Error(msg.str());
  }

  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord(participantGroup, userGroup, mode));
}

void AccessManager::Backend::Storage::removeParticipantGroupAccessRule(
    const std::string& participantGroup, const std::string& userGroup, const std::string& mode) {
  if (!hasParticipantGroupAccessRule(participantGroup, userGroup, mode)) {
        std::ostringstream msg;
        msg << "There is no such participant-group-access-rule ("
            << Logging::Escape(participantGroup) << ", "
            << Logging::Escape(userGroup) << ", "
            << Logging::Escape(mode) << ")";
        throw Error(msg.str());
  }

  mImplementor->raw.insert(ParticipantGroupAccessRuleRecord(participantGroup, userGroup, mode, true));
}


/* Core operations on Columns */
bool AccessManager::Backend::Storage::hasColumn(const std::string& name) {
  return mImplementor->currentRecordExists<ColumnRecord>(c(&ColumnRecord::name) == name);
}

std::set<Column> AccessManager::Backend::Storage::getColumns(const Timestamp& timestamp, const ColumnFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ColumnRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.columns.has_value()
        || in(&ColumnRecord::name, filter.columns.value_or(emptyVector))),
      &ColumnRecord::name)
    | views::transform([](std::string name) {
      return Column(std::move(name));
    })
  );
}

void AccessManager::Backend::Storage::createColumn(const std::string& name) {
  if (hasColumn(name)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->raw.insert(ColumnRecord(name));
  mImplementor->raw.insert(ColumnGroupColumnRecord(name, "*"));
}

void AccessManager::Backend::Storage::removeColumn(const std::string& name) {
  if (!hasColumn(name)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }
  auto guard = mImplementor->raw.transaction_guard();

  const Timestamp now = TimeNow();

  //get associated columnGroups
  auto cgcs = getColumnGroupColumns(now, {.columns = std::vector<std::string>{name}});
  for (const auto& cgc : cgcs) {
    this->removeColumnFromGroup(cgc.column, cgc.columnGroup);
  }

  // Remove metadata
  for (StructureMetadataKey& key : getStructureMetadataKeys(now, StructureMetadataType::Column, name)) {
    removeStructureMetadata(StructureMetadataType::Column, name, std::move(key));
  }

  // Tombstone column
  mImplementor->raw.insert(ColumnRecord(name, true));
  // Remove from column group *
  mImplementor->raw.insert(ColumnGroupColumnRecord(name, "*", true));

  guard.commit();
}

/* Core operations on ColumnGroups */
bool AccessManager::Backend::Storage::hasColumnGroup(const std::string& name) {
  return mImplementor->currentRecordExists<ColumnGroupRecord>(c(&ColumnGroupRecord::name) == name);
}

std::set<ColumnGroup> AccessManager::Backend::Storage::getColumnGroups(const Timestamp& timestamp, const ColumnGroupFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ColumnGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.columnGroups.has_value()
        || in(&ColumnGroupRecord::name, filter.columnGroups.value_or(emptyVector))),
      &ColumnGroupRecord::name)
    | views::transform([](std::string name) {
      return ColumnGroup(std::move(name));
    })
  );
}

void AccessManager::Backend::Storage::createColumnGroup(const std::string& name) {
  if (hasColumnGroup(name)) {
    std::ostringstream msg;
    msg << "Columngroup " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->raw.insert(ColumnGroupRecord(name));
}

void AccessManager::Backend::Storage::removeColumnGroup(const std::string& name, const bool force) {
  if (!hasColumnGroup(name)) {
    std::ostringstream msg;
    msg << "Column-group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }
  auto guard = mImplementor->raw.transaction_guard();

  const Timestamp now = TimeNow();

  auto associatedColumns = getColumnGroupColumns(now, {.columnGroups = std::vector<std::string>{name}});
  auto associatedAccessRules = getColumnGroupAccessRules(now, {.columnGroups = std::vector<std::string>{name}});

  if (force) {
    // remove all associated connections to this columngroup
    //ColumnGroupColumn (cgc)
    for (auto& cgc : associatedColumns) {
      this->removeColumnFromGroup(cgc.column, name);
    }
    for (auto& cgar : associatedAccessRules) {
      this->removeColumnGroupAccessRule(cgar.columnGroup, cgar.userGroup, cgar.mode);
    }
  }
  else if (associatedColumns.size() > 0 || associatedAccessRules.size() > 0) {
    // There where associated columns and or access rules, but force was not given as a parameter. Warn the user.
    std::ostringstream msg;
    msg << "Removing column-group \"" << name << "\" failed due to\n";
    if (associatedColumns.size() > 0) {
      msg << "associated columns:\n";
      for (auto& cgc : associatedColumns) {
        msg << cgc.column << "\n";
      }
    }
    if (associatedAccessRules.size() > 0) {
      msg << "associated column-group-access-rules:\n";
      for (auto& cgar : associatedAccessRules) {
        msg << cgar.mode << " access for usergroup " << cgar.userGroup << "\n";
      }
    }
    throw Error(msg.str());
  }

  // Remove metadata
  for (StructureMetadataKey& key : getStructureMetadataKeys(now, StructureMetadataType::ColumnGroup, name)) {
    removeStructureMetadata(StructureMetadataType::ColumnGroup, name, std::move(key));
  }

  // If we ended up here, it is safe to remove the columnGroup.
  mImplementor->raw.insert(ColumnGroupRecord(name, true));

  guard.commit();
}

/* Core operations on ColumnGroupColumns */
bool AccessManager::Backend::Storage::hasColumnInGroup(
    const std::string& column, const std::string& columnGroup) {
  return mImplementor->currentRecordExists<ColumnGroupColumnRecord>(
    c(&ColumnGroupColumnRecord::column) == column
    && c(&ColumnGroupColumnRecord::columnGroup) == columnGroup);
}

std::set<ColumnGroupColumn> AccessManager::Backend::Storage::getColumnGroupColumns(const Timestamp& timestamp, const ColumnGroupColumnFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ColumnGroupColumnRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.columnGroups.has_value()
        || in(&ColumnGroupColumnRecord::columnGroup, filter.columnGroups.value_or(emptyVector)))
      && (!filter.columns.has_value()
        || in(&ColumnGroupColumnRecord::column, filter.columns.value_or(emptyVector))),
      &ColumnGroupColumnRecord::columnGroup,
      &ColumnGroupColumnRecord::column)
    | views::transform([](auto tuple) {
      auto& [columnGroup, column] = tuple;
      return ColumnGroupColumn(std::move(columnGroup), std::move(column));
    })
  );
}

void AccessManager::Backend::Storage::addColumnToGroup(
    const std::string& column, const std::string& columnGroup) {
  std::ostringstream msg;
  if (hasColumnInGroup(column, columnGroup)) {
    msg << "Column " << Logging::Escape(column) << " is already part of "
        << "column-group " << Logging::Escape(columnGroup);
    throw Error(msg.str());
  }
  if (!hasColumn(column)) {
    msg << "No such column: " << Logging::Escape(column);
    throw Error(msg.str());
  }
  if (!hasColumnGroup(columnGroup)) {
    msg << "No such column-group: " << Logging::Escape(columnGroup);
    throw Error(msg.str());
  }
  mImplementor->raw.insert(ColumnGroupColumnRecord(column, columnGroup));
}

void AccessManager::Backend::Storage::removeColumnFromGroup(
    const std::string& column, const std::string& columnGroup) {
  if (!hasColumnInGroup(column, columnGroup)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(column) << " is not part of "
        << "column-group " << Logging::Escape(columnGroup);
    throw Error(msg.str());
  }
  mImplementor->raw.insert(ColumnGroupColumnRecord(column, columnGroup, true));
}

/* Core operations on ColumnGroup Access Rules */
bool AccessManager::Backend::Storage::hasColumnGroupAccessRule(
    const std::string& columnGroup,
    const std::string& userGroup,
    const std::string& mode) {
  return mImplementor->currentRecordExists<ColumnGroupAccessRuleRecord>(
    c(&ColumnGroupAccessRuleRecord::columnGroup) == columnGroup
    && c(&ColumnGroupAccessRuleRecord::userGroup) == userGroup
    && c(&ColumnGroupAccessRuleRecord::mode) == mode);
}

std::set<ColumnGroupAccessRule> AccessManager::Backend::Storage::getColumnGroupAccessRules(const Timestamp& timestamp, const ColumnGroupAccessRuleFilter& filter) const {
  using namespace std::ranges;
  return RangeToCollection<std::set>(
    mImplementor->getCurrentRecords(
      c(&ColumnGroupAccessRuleRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && (!filter.columnGroups.has_value()
        || in(&ColumnGroupAccessRuleRecord::columnGroup, filter.columnGroups.value_or(emptyVector)))
      && (!filter.userGroups.has_value()
        || in(&ColumnGroupAccessRuleRecord::userGroup, filter.userGroups.value_or(emptyVector)))
      && (!filter.modes.has_value()
        || in(&ColumnGroupAccessRuleRecord::mode, filter.modes.value_or(emptyVector))),
      &ColumnGroupAccessRuleRecord::columnGroup,
      &ColumnGroupAccessRuleRecord::userGroup,
      &ColumnGroupAccessRuleRecord::mode)
    | views::transform([](auto tuple) {
      auto& [columnGroup, userGroup, mode] = tuple;
      return ColumnGroupAccessRule(std::move(columnGroup), std::move(userGroup), std::move(mode));
    })
  );
}

void AccessManager::Backend::Storage::createColumnGroupAccessRule(
    const std::string& columnGroup, const std::string& userGroup, const std::string& mode) {
  std::ostringstream msg;

  if (!hasColumnGroup(columnGroup)) {
        msg << "No such column-group " << Logging::Escape(columnGroup);
        throw Error(msg.str());
  }
  if (mode != "read" && mode != "write" && mode != "read-meta" && mode != "write-meta") {
    msg << "No such mode " << Logging::Escape(mode);
    throw Error(msg.str());
  }
  if (userGroup == UserGroup::DataAdministrator && mode == "read-meta") {
    msg << "Cannot grant explicit \"read-meta\" access rules for "
      << Logging::Escape(userGroup)
      << " because all column-groups are implicitly accessible";
    throw Error(msg.str());
  }
  if (hasColumnGroupAccessRule(columnGroup, userGroup, mode)) {
        msg << "This column-group-access-rule already exists: ("
            << Logging::Escape(columnGroup) << ", "
            << Logging::Escape(userGroup) << ", "
            << Logging::Escape(mode) << ")";
        throw Error(msg.str());
  }

  mImplementor->raw.insert(ColumnGroupAccessRuleRecord(
      columnGroup, userGroup, mode));
}

void AccessManager::Backend::Storage::removeColumnGroupAccessRule(
    const std::string& columnGroup, const std::string& userGroup, const std::string& mode) {
  if (!hasColumnGroupAccessRule(columnGroup, userGroup, mode)) {
        std::ostringstream msg;
        msg << "There is no such column-group-access-rule ("
            << Logging::Escape(columnGroup) << ", "
            << Logging::Escape(userGroup) << ", "
            << Logging::Escape(mode) << ")";
        throw Error(msg.str());
  }

  mImplementor->raw.insert(ColumnGroupAccessRuleRecord(
      columnGroup, userGroup, mode, true));
}

/* Core operations on Column Name Mappings */
std::vector<ColumnNameMapping> AccessManager::Backend::Storage::getAllColumnNameMappings() const {
  std::vector<ColumnNameMapping> result;
  for (auto& record : mImplementor->raw.iterate<ColumnNameMappingRecord>()) {
    result.push_back(record.toLiveObject());
  }
  return result;
}

std::optional<ColumnNameMapping> AccessManager::Backend::Storage::getColumnNameMapping(const ColumnNameSection& original) const {
  // Would have liked to use raw.get_no_throw<ColumnNameMappingRecord>(original.getValue()), but I can't get primary key string columns to work
  auto found = mImplementor->raw.get_all<ColumnNameMappingRecord>(where((c(&ColumnNameMappingRecord::original) == original.getValue())));
  assert(found.size() < 2);
  if (found.empty()) {
    return std::nullopt;
  }
  return found.front().toLiveObject();
}

void AccessManager::Backend::Storage::createColumnNameMapping(const ColumnNameMapping& mapping) {
  auto record = ColumnNameMappingRecord::FromLiveObject(mapping);
  try {
    mImplementor->raw.insert(record);
  }
  catch (const std::system_error&) {
    if (getColumnNameMapping(mapping.original)) {
      throw Error("A mapping for that original column name already exists");
    }
    throw;
  }
}

void AccessManager::Backend::Storage::updateColumnNameMapping(const ColumnNameMapping& mapping) {
  auto record = ColumnNameMappingRecord::FromLiveObject(mapping);
  // Would have liked to use raw.update(record), but I can't get primary key string columns to work
  if (!getColumnNameMapping(mapping.original)) {
    throw Error("No mapping found for that original name");
  }
  mImplementor->raw.update_all(set(c(&ColumnNameMappingRecord::mapped) = record.mapped),
    where(c(&ColumnNameMappingRecord::original) == record.original));
  assert(getColumnNameMapping(mapping.original).has_value());
  assert(getColumnNameMapping(mapping.original)->original.getValue() == record.original);
  assert(getColumnNameMapping(mapping.original)->mapped.getValue() == record.mapped);
}

void AccessManager::Backend::Storage::deleteColumnNameMapping(const ColumnNameSection& original) {
  // Would have liked to use raw.remove<ColumnNameMapping>(original.getValue()),
  // but I can't get primary key string columns to work
  if (!getColumnNameMapping(original)) {
    throw Error("No mapping found for that original name");
  }
  mImplementor->raw.remove_all<ColumnNameMappingRecord>(where(c(&ColumnNameMappingRecord::original) == original.getValue()));
  assert(!getColumnNameMapping(original));
}

void AccessManager::Backend::Storage::ensureNoUserData() const {
  int countUserIds = mImplementor->raw.count<UserIdRecord>();
  int countUserGroups = mImplementor->raw.count<UserGroupRecord>();
  int countUserGroupUsers = mImplementor->raw.count<UserGroupUserRecord>();

  if (countUserIds > 0 || countUserGroups > 0 || countUserGroupUsers > 0) {
    std::ostringstream msg;
    msg << "Cannot perform userDb migration. There is already user data in the storage: ";
    msg << countUserIds << " userIds, ";
    msg << countUserGroups << " userGroups, ";
    msg << countUserGroupUsers << " userGroupUsers";
    throw Error(msg.str());
  }
}

MigrateUserDbToAccessManagerResponse AccessManager::Backend::Storage::migrateUserDb(
    const std::filesystem::path& dbPath) {
#if BUILD_HAS_DEBUG_FLAVOR()
  try {
    ensureNoUserData();
  }
  catch (...) {
    assert(false); // This should already have been checked on a higher level (AccessManager::handleMigrateUserDbToAccessManagerRequest).
  }
#endif
  auto authserverStorage = std::make_shared<LegacyAuthserverStorage>(dbPath);
  auto transactionGuard = std::make_shared<internal::transaction_guard_t>(mImplementor->raw.transaction_guard());

  // Migrate UserIdRecords
  for(auto& userId : authserverStorage->getUserIdRecords()) {
    mImplementor->raw.insert(userId);
  }

  // Migrate UserGroupRecords
  int64_t userGroupId = 1; //Migration may only happen when no UserGroup data is present in the accessmanager storage. So we start with userGroupId 1
  for(auto userGroup : authserverStorage->getUserGroupRecords()) {
    auto id = findUserGroupId(userGroup.name);
    if (!userGroup.tombstone) {
      if (id) { //The current record is for a modification of an existing group
        userGroup.userGroupId = *id;
      }
      else {
        userGroup.userGroupId = userGroupId++;
      }
    }
    else {
      // We are rebuilding the userGroup table in chronological order. So when we are looking for an existing userGroup,
      // we just want the latest one that is already added to the storage. No need to worry about timestamps here.
      if (!id) {
        std::ostringstream msg;
        msg << "Encountered a tombstone for user group " << userGroup.name << " but this user group does not exist";
        throw Error(msg.str());
      }
      userGroup.userGroupId = *id;
    }
    mImplementor->raw.insert(userGroup);
  }

  // Migrate UserGroupUserRecords
  for(auto& legacyUserGroupUser : authserverStorage->getUserGroupUserRecords()) {
    UserGroupUserRecord userGroupUser(legacyUserGroupUser);
    // A usergroup with a certain name can be created, removed and then be created again, resulting in a different userGroupId.
    // So we want to make sure that we find the userGroupId for the correct timestamp
    auto userGroupId = findUserGroupId(legacyUserGroupUser.group, Timestamp(milliseconds{userGroupUser.timestamp}));
    if (!userGroupId) {
      std::ostringstream msg;
      msg << "Encountered a userGroupUser record for user group " << legacyUserGroupUser.group << " but this user group does not exist for the timestamp of the userGroupUser record";
      throw Error(msg.str());
    }
    userGroupUser.userGroupId = *userGroupId;
    mImplementor->raw.insert(userGroupUser);
  }

  transactionGuard->commit();
  return MigrateUserDbToAccessManagerResponse();
}

int64_t AccessManager::Backend::Storage::getNextInternalUserId() const {
  auto currentMax = mImplementor->raw.max(&UserIdRecord::internalUserId);
  if (currentMax) {
    return *currentMax + 1;
  }
  return 1;
}

int64_t AccessManager::Backend::Storage::getNextUserGroupId() const {
  auto currentMax = mImplementor->raw.max(&UserGroupRecord::userGroupId);
  if (currentMax) {
    return *currentMax + 1;
  }
  return 1;
}

int64_t AccessManager::Backend::Storage::createUser(std::string identifier) {
  int64_t internalUserId = getNextInternalUserId();
  addIdentifierForUser(internalUserId, std::move(identifier), UserIdFlags::isDisplayId);
  return internalUserId;
}

void AccessManager::Backend::Storage::removeUser(std::string_view uid) {
  int64_t internalUserId = getInternalUserId(uid);
  removeUser(internalUserId);
}

void AccessManager::Backend::Storage::removeUser(int64_t internalUserId) {
  auto groups = getUserGroupsForUser(internalUserId);
  if(!groups.empty()) {
    if(groups.size() > 10)
      throw Error("User is still in " + std::to_string(groups.size()) + " user groups");
    std::ostringstream oss;
    oss << "User is still in user groups: ";
    bool first = true;
    for(auto& group : groups) {
      if (!first) {
        oss << ", ";
        first = false;
      }
      oss << group.mName;
    }
    throw Error(oss.str());
  }

  // Remove metadata
  for (StructureMetadataKey& key : getStructureMetadataKeys(TimeNow(), StructureMetadataType::User, internalUserId)) {
    removeStructureMetadata(StructureMetadataType::User, internalUserId, std::move(key));
  }

  for(auto& uid : getAllIdentifiersForUser(internalUserId))
    mImplementor->raw.insert(UserIdRecord(internalUserId, uid, UserIdFlags::none, true));
}

void AccessManager::Backend::Storage::addIdentifierForUser(std::string_view uid, std::string identifier, UserIdFlags flags) {
  int64_t internalUserId = getInternalUserId(uid);
  addIdentifierForUser(internalUserId, std::move(identifier), flags);
}

void AccessManager::Backend::Storage::addIdentifierForUser(int64_t internalUserId, std::string identifier, UserIdFlags flags) {
  if (findInternalUserId(identifier)) {
    throw Error("The user identifier already exists");
  }
  mImplementor->raw.insert(UserIdRecord(internalUserId, std::move(identifier), flags));
}

void AccessManager::Backend::Storage::removeIdentifierForUser(std::string identifier) {
  int64_t internalUserId = getInternalUserId(identifier);
  removeIdentifierForUser(internalUserId, std::move(identifier));
}

void AccessManager::Backend::Storage::removeIdentifierForUser(int64_t internalUserId, std::string identifier) {
  auto identifiers = getAllIdentifiersForUser(internalUserId);
  if(identifiers.size() == 0)
    throw Error("The user does not exist");
  if(identifiers.size() == 1)
    throw Error("You are trying to remove the last identifier for a user. This will make it impossible to "
      "address that user, and is therefore not allowed. Instead, you can remove the user, if that is the intention");

  if(!identifiers.contains(identifier)) {
    throw Error("The given identifier does not exist for the given internalUserId");
  }

  if (getDisplayIdentifierForUser(internalUserId) == identifier) {
    throw Error("Cannot remove the display identifier for a user. First set a different display identifier, then you can remove this one.");
  }

  mImplementor->raw.insert(UserIdRecord(internalUserId, std::move(identifier), UserIdFlags::none, true));
}

std::optional<int64_t> AccessManager::Backend::Storage::findInternalUserId(std::string_view identifier, Timestamp at) const {
  return RangeToOptional(
    mImplementor->getCurrentRecords(
      c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
      && c(&UserIdRecord::identifier) == identifier,
      &UserIdRecord::internalUserId)
  );
}

int64_t AccessManager::Backend::Storage::getInternalUserId(std::string_view identifier, Timestamp at) const {
  std::optional<int64_t> internalUserId = findInternalUserId(identifier, at);
  if(!internalUserId) {
    throw Error("Could not find user id");
  }
  return internalUserId.value();
}

std::optional<int64_t> AccessManager::Backend::Storage::findInternalUserId(const std::vector<std::string>& identifiers, Timestamp at) const {
  using namespace std::ranges;
  return RangeToOptional(
    RangeToCollection<std::unordered_set>( // Merge duplicates
      mImplementor->getCurrentRecords(
        c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
        && in(&UserIdRecord::identifier, identifiers),
        &UserIdRecord::internalUserId)
    )
  );
}

std::unordered_set<std::string> AccessManager::Backend::Storage::getAllIdentifiersForUser(int64_t internalUserId, Timestamp at) const {
  return RangeToCollection<std::unordered_set>(
    mImplementor->getCurrentRecords(
      c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
      && c(&UserIdRecord::internalUserId) == internalUserId,
      &UserIdRecord::identifier)
  );
}

std::optional<std::string> AccessManager::Backend::Storage::getPrimaryIdentifierForUser(int64_t internalUserId, Timestamp at) const {
  using namespace pep::database;
  return RangeToOptional(mImplementor->getCurrentRecords(c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
        && c(&UserIdRecord::internalUserId) == internalUserId, having(c(&UserIdRecord::isPrimaryId) == true), &UserIdRecord::identifier));
}

std::optional<std::string> AccessManager::Backend::Storage::getDisplayIdentifierForUser(int64_t internalUserId, Timestamp at) const {
  using namespace pep::database;
  return RangeToOptional(mImplementor->getCurrentRecords(c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
        && c(&UserIdRecord::internalUserId) == internalUserId, having(c(&UserIdRecord::isDisplayId) == true), &UserIdRecord::identifier));
}

void AccessManager::Backend::Storage::setPrimaryIdentifierForUser(std::string uid) {
  auto internalId = getInternalUserId(uid);
  setPrimaryIdentifierForUser(internalId, std::move(uid));
}

void AccessManager::Backend::Storage::setPrimaryIdentifierForUser(int64_t internalUserId, std::string uid) {
  assert(getInternalUserId(uid) == internalUserId);
  auto currentPrimaryIdentifier = getPrimaryIdentifierForUser(internalUserId);
  if (currentPrimaryIdentifier == uid) {
    throw Error("This user identifier is already the primary identifier.");
  }

  auto currentDisplayIdentifier = getDisplayIdentifierForUser(internalUserId);
  auto transactionGuard = mImplementor->raw.transaction_guard();
  if (currentPrimaryIdentifier) {
    mImplementor->raw.insert(UserIdRecord(internalUserId, *currentPrimaryIdentifier, currentDisplayIdentifier==*currentPrimaryIdentifier ? UserIdFlags::isDisplayId : UserIdFlags::none));
  }
  UserIdFlags flags = UserIdFlags::isPrimaryId;
  if (currentDisplayIdentifier == uid) {
    flags |= UserIdFlags::isDisplayId;

  }
  mImplementor->raw.insert(UserIdRecord(internalUserId, std::move(uid), flags));
  transactionGuard.commit();
}

void AccessManager::Backend::Storage::unsetPrimaryIdentifierForUser(std::string_view uid) {
  auto internalId = getInternalUserId(uid);
  unsetPrimaryIdentifierForUser(internalId, uid);
}

void AccessManager::Backend::Storage::unsetPrimaryIdentifierForUser(int64_t internalUserId, std::string_view uid) {
  assert(getInternalUserId(uid) == internalUserId);
  auto currentPrimaryIdentifier = getPrimaryIdentifierForUser(internalUserId);
  if (currentPrimaryIdentifier != uid) {
    throw Error("This user identifier is not the current primary identifier.");
  }

  auto transactionGuard = mImplementor->raw.transaction_guard();
  auto currentDisplayIdentifier = getDisplayIdentifierForUser(internalUserId);
  mImplementor->raw.insert(UserIdRecord(internalUserId, std::move(*currentPrimaryIdentifier), currentDisplayIdentifier==uid ? UserIdFlags::isDisplayId : UserIdFlags::none));
  transactionGuard.commit();
}

void AccessManager::Backend::Storage::setDisplayIdentifierForUser(std::string uid) {
  auto internalId = getInternalUserId(uid);
  setDisplayIdentifierForUser(internalId, std::move(uid));
}

void AccessManager::Backend::Storage::setDisplayIdentifierForUser(int64_t internalUserId, std::string uid) {
  assert(getInternalUserId(uid) == internalUserId);
  auto currentDisplayIdentifier = getDisplayIdentifierForUser(internalUserId);
  if (currentDisplayIdentifier == uid) {
    throw Error("This user identifier is already the display identifier.");
  }

  auto currentPrimaryIdentifier = getPrimaryIdentifierForUser(internalUserId);
  auto transactionGuard = mImplementor->raw.transaction_guard();
  if (currentDisplayIdentifier) {
    mImplementor->raw.insert(UserIdRecord(internalUserId, *currentDisplayIdentifier, currentPrimaryIdentifier==*currentDisplayIdentifier ? UserIdFlags::isPrimaryId : UserIdFlags::none));
  }
  UserIdFlags flags = UserIdFlags::isDisplayId;
  if (currentPrimaryIdentifier==uid) {
    flags |= UserIdFlags::isPrimaryId;
  }
  mImplementor->raw.insert(UserIdRecord(internalUserId, std::move(uid), flags));
  transactionGuard.commit();
}

std::optional<int64_t> AccessManager::Backend::Storage::findUserGroupId(std::string_view name, Timestamp at) const {
  auto records = mImplementor->getCurrentRecords(
      c(&UserGroupRecord::name) == name
      && c(&UserGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at),
      &UserGroupRecord::seqno, &UserGroupRecord::userGroupId);

  if (records.begin() == records.end()) {
    return std::nullopt;
  }

  auto [seqno, userGroupId] = std::ranges::max(records, {}, [](const std::tuple<int64_t, int64_t>& tuple) {
    auto [seqno, userGroupId] = tuple;
    return seqno;
  });
  return userGroupId;
}

int64_t AccessManager::Backend::Storage::getUserGroupId(std::string_view name, Timestamp at) const {
  std::optional<int64_t> userGroupId = findUserGroupId(name);
  if(!userGroupId) {
    throw Error("Could not find usergroup");
  }
  return userGroupId.value();
}

std::optional<std::string> AccessManager::Backend::Storage::getUserGroupName(int64_t userGroupId, Timestamp at) const {
  return RangeToOptional(
    mImplementor->getCurrentRecords(
      c(&UserGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at)
      && c(&UserGroupRecord::userGroupId) == userGroupId,
      &UserGroupRecord::name)
  );
}

std::vector<UserGroup> AccessManager::Backend::Storage::getUserGroupsForUser(int64_t internalUserId, Timestamp at) const {
  using namespace std::ranges;
  std::vector<int64_t> groupIds = RangeToCollection<std::vector>(
    mImplementor->getCurrentRecords(
      c(&UserGroupUserRecord::internalUserId) == internalUserId
      && c(&UserGroupUserRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at),
      &UserGroupUserRecord::userGroupId)
  );

  return RangeToCollection<std::vector>(
    mImplementor->getCurrentRecords(
      in(&UserGroupRecord::userGroupId, groupIds)
      && c(&UserGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(at),
        &UserGroupRecord::name, &UserGroupRecord::maxAuthValiditySeconds)
  | views::transform([](auto tuple) {
    auto& [name, maxAuthValiditySeconds] = tuple;
    return UserGroup(name, maxAuthValiditySeconds ? std::make_optional(std::chrono::seconds(*maxAuthValiditySeconds)) : std::nullopt);
  })
  );
}

bool AccessManager::Backend::Storage::hasUserGroup(std::string_view name) const {
  return mImplementor->currentRecordExists<UserGroupRecord>(c(&UserGroupRecord::name) == name);
}

std::optional<std::chrono::seconds> AccessManager::Backend::Storage::getMaxAuthValidity(const std::string& group, Timestamp at) const {
  using namespace std::ranges;
  auto result = RangeToOptional(
    mImplementor->getCurrentRecords(
      c(&UserGroupRecord::name) == group,
      &UserGroupRecord::maxAuthValiditySeconds)
    | views::transform(to_optional_seconds)
  );
  if (!result) {
    std::ostringstream msg;
    msg << "Could not find group " << Logging::Escape(group);
    throw Error(msg.str());
  }
  return *result;
}

bool AccessManager::Backend::Storage::userInGroup(std::string_view uid, std::string_view group) const {
  int64_t internalUserId = getInternalUserId(uid);
  return userInGroup(internalUserId, group);
}

bool AccessManager::Backend::Storage::userInGroup(int64_t internalUserId, std::string_view group) const {
  return userInGroup(internalUserId, getUserGroupId(group));
}

bool AccessManager::Backend::Storage::userInGroup(int64_t internalUserId,
                                                  int64_t userGroupId) const {
  return mImplementor->currentRecordExists<UserGroupUserRecord>(
    c(&UserGroupUserRecord::internalUserId) == internalUserId
    && c(&UserGroupUserRecord::userGroupId) == userGroupId);
}

bool AccessManager::Backend::Storage::userGroupIsEmpty(int64_t userGroupId) const {
  return !mImplementor->currentRecordExists<UserGroupUserRecord>(
    c(&UserGroupUserRecord::userGroupId) == userGroupId);
}

int64_t AccessManager::Backend::Storage::createUserGroup(UserGroup userGroup) {
  if (hasUserGroup(userGroup.mName)) {
    std::ostringstream msg;
    msg << "User group " << Logging::Escape(userGroup.mName) << " already exists";
    throw Error(msg.str());
  }
  int64_t userGroupId = getNextUserGroupId();
  mImplementor->raw.insert(UserGroupRecord(userGroupId, std::move(userGroup.mName), to_optional_uint64(userGroup.mMaxAuthValidity)));
  return userGroupId;
}

void AccessManager::Backend::Storage::modifyUserGroup(UserGroup userGroup) {
  if (!hasUserGroup(userGroup.mName)) {
    std::ostringstream msg;
    msg << "User group " << Logging::Escape(userGroup.mName) << " doesn't exist";
    throw Error(msg.str());
  }

  auto userGroupId = getUserGroupId(userGroup.mName); // Prevent use-after-move
  mImplementor->raw.insert(UserGroupRecord(userGroupId, std::move(userGroup.mName), to_optional_uint64(userGroup.mMaxAuthValidity)));
}

void AccessManager::Backend::Storage::removeUserGroup(std::string name) {
  std::optional<int64_t> userGroupId = findUserGroupId(name);
  if (!userGroupId) {
    std::ostringstream msg;
    msg << "group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }

  if (!userGroupIsEmpty(*userGroupId)) {
    std::ostringstream msg;
    msg << "Group " << Logging::Escape(name) << " still has users. Group will not be removed";
    throw Error(msg.str());
  }

  // Remove metadata
  for (StructureMetadataKey& key : getStructureMetadataKeys(TimeNow(), StructureMetadataType::UserGroup, *userGroupId)) {
    removeStructureMetadata(StructureMetadataType::UserGroup, *userGroupId, std::move(key));
  }

  mImplementor->raw.insert(UserGroupRecord(*userGroupId, name, std::nullopt, true));
}

void AccessManager::Backend::Storage::addUserToGroup(std::string_view uid, std::string group) {
  int64_t internalUserId = getInternalUserId(uid);
  addUserToGroup(internalUserId, std::move(group));
}

void AccessManager::Backend::Storage::addUserToGroup(int64_t internalUserId, std::string group) {
  std::ostringstream msg;
  if (userInGroup(internalUserId, group)) {
    msg << "User is already in group: " << Logging::Escape(group);
    throw Error(msg.str());
  }

  std::optional<int64_t> userGroupId = findUserGroupId(group);
  if (!userGroupId) {
    msg << "No such group: " << Logging::Escape(group);
    throw Error(msg.str());
  }

  mImplementor->raw.insert(UserGroupUserRecord(internalUserId, *userGroupId));
}

void AccessManager::Backend::Storage::removeUserFromGroup(std::string_view uid, std::string group) {
  int64_t internalUserId = getInternalUserId(uid);
  removeUserFromGroup(internalUserId, std::move(group));
}

void AccessManager::Backend::Storage::removeUserFromGroup(int64_t internalUserId, std::string group) {
  int64_t userGroupId = getUserGroupId(group);
  if (!userInGroup(internalUserId, userGroupId)) {
    std::ostringstream msg;
    msg << "This user is not part of group " << Logging::Escape(group);
    throw Error(msg.str());
  }

  mImplementor->raw.insert(UserGroupUserRecord(internalUserId, userGroupId, true));
}

UserQueryResponse AccessManager::Backend::Storage::executeUserQuery(const UserQuery& query) {
  using namespace std::ranges;

  auto timestamp = query.mAt ? *query.mAt : TimeNow();

  // Select groups matching group filter
  auto groups = RangeToCollection<std::map<int64_t, UserGroup>>(
    mImplementor->getCurrentRecords(
      c(&UserGroupRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && instr(&UserGroupRecord::name, query.mGroupFilter) /*true if filter is empty*/,
      &UserGroupRecord::userGroupId,
      &UserGroupRecord::name,
      &UserGroupRecord::maxAuthValiditySeconds)
    | views::transform([](auto tuple) {
      auto& [userGroupId, name, maxAuthValiditySeconds] = tuple;
      return std::make_pair(userGroupId, UserGroup(std::move(name), to_optional_seconds(maxAuthValiditySeconds)));
    })
  );

  std::map<int64_t, QRUser> usersInfo;
  // List users matching user filter
  for (auto internalId: mImplementor->getCurrentRecords(
         c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
         && instr(&UserIdRecord::identifier, query.mUserFilter) /*true if filter is empty*/,
         &UserIdRecord::internalUserId)) {
    // Add internalId, we add all identifiers below
    usersInfo.try_emplace(internalId);
  }

  std::unordered_set<int64_t> groupsWithUsers;
  // List group memberships for filtered groups & users
  for (auto tuple: mImplementor->getCurrentRecords(
         c(&UserGroupUserRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
         && (query.mGroupFilter.empty()
           || in(&UserGroupUserRecord::userGroupId,
             // Avoid passing list to query when not filtered
             RangeToVector(views::keys(!query.mGroupFilter.empty() ? groups : Default<decltype(groups)>))) )
         && (query.mUserFilter.empty()
           || in(&UserGroupUserRecord::internalUserId,
             // Avoid passing list to query when not filtered
             RangeToVector(views::keys(!query.mUserFilter.empty() ? usersInfo : Default<decltype(usersInfo)>)))),
         &UserGroupUserRecord::userGroupId,
         &UserGroupUserRecord::internalUserId)) {
    auto& [userGroupId, internalUserId] = tuple;
    assert(groups.contains(userGroupId));
    usersInfo.at(internalUserId).mGroups.push_back(groups.at(userGroupId).mName);
    groupsWithUsers.insert(userGroupId);
  }

  // Backpropagate user filter to filter groups
  if (!query.mUserFilter.empty()) {
    // Remove groups without selected users
    std::erase_if(groups, [&groupsWithUsers](const std::pair<int64_t, UserGroup>& keyValuePair) {
      return !groupsWithUsers.contains(keyValuePair.first);
    });
  }
  // Backpropagate group filter to filter users
  if (!query.mGroupFilter.empty()) {
    erase_if(usersInfo, [](const std::pair<int64_t, QRUser>& userInfo) {
      return userInfo.second.mGroups.empty();
    });
  }

  // Fetch all identifiers for the selected users,
  //  not just the ones that satisfy the specific user identifier filter
  for (auto tuple: mImplementor->getCurrentRecords(
         c(&UserIdRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
         && in(&UserIdRecord::internalUserId, RangeToVector(views::keys(usersInfo))),
         &UserIdRecord::internalUserId, &UserIdRecord::identifier, &UserIdRecord::isPrimaryId, &UserIdRecord::isDisplayId)) {
    auto& [internalId, identifier, isPrimaryId, isDisplayId] = tuple;

    QRUser& user = usersInfo.at(internalId);
    if (isDisplayId) {
      user.mDisplayId = identifier;
    }
    if (isPrimaryId) {
      user.mPrimaryId = identifier;
    }
    if (!isPrimaryId && !isDisplayId) {
      user.mOtherUids.push_back(std::move(identifier));
    }
  }

  return UserQueryResponse{
    RangeToVector(std::move(usersInfo) | views::values),
    RangeToVector(std::move(groups) | views::values)
  };
}

/* Core operations on Metadata */
std::optional<int64_t> AccessManager::Backend::Storage::findInternalSubjectId(StructureMetadataType subjectType, std::string_view subject, Timestamp at) const {
  assert(HasInternalId(subjectType));
  switch (subjectType) {
  case StructureMetadataType::User:
    return findInternalUserId(subject, at);
  case StructureMetadataType::UserGroup:
    return findUserGroupId(subject, at);
    break;
  default:
    throw std::logic_error("Specificed subjectType does not have an internalId");
  }
}

int64_t AccessManager::Backend::Storage::getInternalSubjectId(StructureMetadataType subjectType, std::string_view subject, Timestamp at) const {
  assert(HasInternalId(subjectType));
  switch (subjectType) {
  case StructureMetadataType::User:
    return getInternalUserId(subject, at);
  case StructureMetadataType::UserGroup:
    return getUserGroupId(subject, at);
    break;
  default:
    throw std::logic_error("Specificed subjectType does not have an internalId");
  }
}

std::optional<std::string> AccessManager::Backend::Storage::getSubjectForInternalId(
  StructureMetadataType subjectType, int64_t internalId, Timestamp at) const {
  assert(HasInternalId(subjectType));
  switch (subjectType) {
  case StructureMetadataType::User: {
    auto identifier = getDisplayIdentifierForUser(internalId, at);
    if (identifier)
      return identifier;
    auto identifiers = getAllIdentifiersForUser(internalId, at);
    if (identifiers.size() > 0) {
      return std::ranges::min(getAllIdentifiersForUser(internalId, at));
    }
    return std::nullopt;
  }
  case StructureMetadataType::UserGroup:
    return getUserGroupName(internalId, at);
  default:
    throw std::logic_error("Specificed subjectType does not have an internalId");
  }
}

std::vector<StructureMetadataKey> AccessManager::Backend::Storage::getStructureMetadataKeys(
    const Timestamp timestamp,
    StructureMetadataType subjectType,
    std::string_view subject) const {
  if (HasInternalId(subjectType)) {
    return getStructureMetadataKeys(timestamp, subjectType, getInternalSubjectId(subjectType, subject, timestamp));
  }
  using namespace std::ranges;
  return RangeToVector(
    mImplementor->getCurrentRecords(
      c(&StructureMetadataRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && c(&StructureMetadataRecord::subjectType) == ToUnderlying(subjectType)
      && c(&StructureMetadataRecord::subject) == subject,
      &StructureMetadataRecord::metadataGroup,
      &StructureMetadataRecord::subkey)
    | views::transform([](auto tuple) {
      auto& [metadataGroup, subkey] = tuple;
      return StructureMetadataKey(std::move(metadataGroup), std::move(subkey));
    })
  );
}

std::vector<StructureMetadataKey> AccessManager::Backend::Storage::getStructureMetadataKeys(
    const Timestamp timestamp,
    StructureMetadataType subjectType,
    int64_t internalSubjectId) const {
  assert(HasInternalId(subjectType));
  using namespace std::ranges;
  return RangeToVector(
    mImplementor->getCurrentRecords(
      c(&StructureMetadataRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && c(&StructureMetadataRecord::subjectType) == ToUnderlying(subjectType)
      && c(&StructureMetadataRecord::internalSubjectId) == internalSubjectId,
      &StructureMetadataRecord::metadataGroup,
      &StructureMetadataRecord::subkey)
    | views::transform([](auto tuple) {
      auto& [metadataGroup, subkey] = tuple;
      return StructureMetadataKey(std::move(metadataGroup), std::move(subkey));
    })
  );
}

std::vector<StructureMetadataEntry> AccessManager::Backend::Storage::getStructureMetadata(const Timestamp timestamp, StructureMetadataType subjectType, const StructureMetadataFilter& filter) const {
  using namespace std::ranges;

  bool hasInternalId = HasInternalId(subjectType);

  std::vector<std::reference_wrapper<const std::string>> metadataGroupFilters;
  std::vector<std::string> metadataKeyFilters;
  for (const auto& key : filter.keys) {
    if (key.metadataGroup.empty()) { throw Error("metadataGroup in filter cannot be empty"); }
    if (key.subkey.empty()) { metadataGroupFilters.emplace_back(key.metadataGroup); }
    else { metadataKeyFilters.emplace_back(key.toString()); }
  }

  std::unordered_map<int64_t, std::string> internalSubjectIds; //This is a map, so we can translate the internalIds back to a subject, as specified in the filter.
  if (hasInternalId) {
    internalSubjectIds.reserve(filter.subjects.size());
    for (auto& subject : filter.subjects) {
      auto internalSubjectId = findInternalSubjectId(subjectType, subject, timestamp);
      if (internalSubjectId) {
        internalSubjectIds.try_emplace(*internalSubjectId, subject);
      }
    }
  }

  return RangeToVector(
    mImplementor->getCurrentRecords(
      c(&StructureMetadataRecord::timestamp) <= TicksSinceEpoch<milliseconds>(timestamp)
      && c(&StructureMetadataRecord::subjectType) == ToUnderlying(subjectType)
      // If we have no subject filters, we return all subjects. If we do have subject filters, we either need to check directly, or via internalId.
      // If we have a non-empty filter, it is still possible that internalSubjectIds is empty. because no subjects match the filter.
      // But in that case, we don't want to return everything. That is why we don't check the emptiness of internalSubjectIds.
      && (filter.subjects.empty() || hasInternalId || in(&StructureMetadataRecord::subject, filter.subjects))
      && (filter.subjects.empty() || !hasInternalId || in(&StructureMetadataRecord::internalSubjectId, RangeToVector(views::keys(internalSubjectIds))))
      && ((metadataGroupFilters.empty() && metadataKeyFilters.empty())
        || in(&StructureMetadataRecord::metadataGroup, metadataGroupFilters)
        || in(conc(conc(&StructureMetadataRecord::metadataGroup, ":"), &StructureMetadataRecord::subkey), metadataKeyFilters)),
      &StructureMetadataRecord::subject,
      &StructureMetadataRecord::internalSubjectId,
      &StructureMetadataRecord::metadataGroup,
      &StructureMetadataRecord::subkey,
      &StructureMetadataRecord::value)
    | views::transform([this, &internalSubjectIds, subjectType, timestamp](auto tuple) {
      auto& [subject, internalSubjectId, metadataGroup, subkey, value] = tuple;
      if (HasInternalId(subjectType)) {
        assert(internalSubjectId.has_value());
        if (internalSubjectIds.size() > 0) {
          // If internalSubjectIds is non-empty, the current internalSubjectId should be in the map.
          subject=internalSubjectIds.at(*internalSubjectId);
        }
        else {
          // otherwise, we had an empty filter. We'll need to look up a matching subject in the database.
          auto foundSubject = getSubjectForInternalId(subjectType, *internalSubjectId, timestamp);
          if (!foundSubject) {
            throw std::runtime_error("Encountered an internalSubjectId, for which no subject could be found.");
          }
          subject = *foundSubject;
        }
      }
      return StructureMetadataEntry{
        .subjectKey = {
          .subject = std::move(subject),
          .key = {std::move(metadataGroup), std::move(subkey)}
        },
        .value = RangeToCollection<std::string>(value)
      };
    })
  );
}

void AccessManager::Backend::Storage::setStructureMetadata(StructureMetadataType subjectType, std::string subject, StructureMetadataKey key, std::string_view value) {
  bool subjectExists = false;
  std::optional<int64_t> internalSubjectId;
  switch (subjectType) {
  case StructureMetadataType::Column:           subjectExists = hasColumn(subject); break;
  case StructureMetadataType::ColumnGroup:      subjectExists = hasColumnGroup(subject); break;
  case StructureMetadataType::ParticipantGroup: subjectExists = hasParticipantGroup(subject); break;
  case StructureMetadataType::User:
  case StructureMetadataType::UserGroup:
    internalSubjectId = findInternalSubjectId(subjectType, subject);
    subjectExists = internalSubjectId.has_value();
    break;
  }
  if (!subjectExists) {
    std::ostringstream msg;
    msg << Logging::Escape(subject) << " does not exist";
    throw Error(std::move(msg).str());
  }

  if (internalSubjectId) {
    mImplementor->raw.insert(StructureMetadataRecord(
      subjectType,
      *internalSubjectId,
      std::move(key.metadataGroup),
      std::move(key.subkey),
      std::vector(value.begin(), value.end())));
  }
  else {
    mImplementor->raw.insert(StructureMetadataRecord(
      subjectType,
      std::move(subject),
      std::move(key.metadataGroup),
      std::move(key.subkey),
      std::vector(value.begin(), value.end())));
  }
}

void AccessManager::Backend::Storage::removeStructureMetadata(StructureMetadataType subjectType, std::string subject, StructureMetadataKey key) {
  if (HasInternalId(subjectType)) {
    int64_t internalId = getInternalSubjectId(subjectType, subject);
    removeStructureMetadata(subjectType, internalId, key);
    return;
  }

  const auto keys = getStructureMetadataKeys(TimeNow(), subjectType, subject);
  if (std::ranges::find(keys, key) == keys.end()) {
    std::ostringstream msg;
    msg << Logging::Escape(subject) << " does not exist or does not contain metadata key "
        << Logging::Escape(key.toString());
    throw Error(std::move(msg).str());
  }
  mImplementor->raw.insert(StructureMetadataRecord(
      subjectType,
      std::move(subject),
      std::move(key.metadataGroup),
      std::move(key.subkey),
      {},
      true));
}

void AccessManager::Backend::Storage::removeStructureMetadata(StructureMetadataType subjectType, int64_t internalSubjectId, StructureMetadataKey key) {
  assert(HasInternalId(subjectType));
  const auto keys = getStructureMetadataKeys(TimeNow(), subjectType, internalSubjectId);

  if (std::ranges::find(keys, key) == keys.end()) {
    std::ostringstream msg;
    msg << "subject does not exist or does not contain metadata key "
        << Logging::Escape(key.toString());
    throw Error(std::move(msg).str());
  }

  mImplementor->raw.insert(StructureMetadataRecord(
      subjectType,
      internalSubjectId,
      std::move(key.metadataGroup),
      std::move(key.subkey),
      {},
      true));
}

}
