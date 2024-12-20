// Storage class for the access manager.

#include <pep/accessmanager/Storage.hpp>

#include <pep/auth/UserGroups.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <iterator>
#include <set>

#include <sqlite_orm/sqlite_orm.h>

#include <filesystem>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>
#include <unordered_set>

namespace pep {

const std::vector<std::string> emptyVector{}; // Used as a default for optional vectors.
using namespace sqlite_orm;

static const std::string LOG_TAG("AccessManager::Backend::Storage");

// This function defines the database scheme used.
inline auto am_create_db(const std::string& path) {
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
      make_column("mapped", &ColumnNameMappingRecord::mapped))
    );
}

class AccessManager::Backend::Storage::Implementor {
public:
  Implementor(const std::string& path) : m(am_create_db(path)) {}
  decltype(am_create_db("")) m;
};

void AccessManager::Backend::Storage::ensureInitialized() {
  LOG(LOG_TAG, info) << "Synching database schema ...";
  try {
    for(const auto& p : mImplementor->m.sync_schema(true)) {
      if (p.second == sync_schema_result::already_in_sync)
        continue;
      LOG(LOG_TAG, warning) << "  " << p.first << ": " << p.second;
    }
  } catch (const std::system_error& e) {
    LOG(LOG_TAG, error) << "  failed: " << e.what();
    throw;
  }

  if (mImplementor->m.count<ColumnGroupRecord>() != 0)
    return;

  LOG(LOG_TAG, warning) << "Database seems uninitialized.  Initializing ...";

  // Column groups
  mImplementor->m.insert(ColumnGroupRecord("*"));
  mImplementor->m.insert(ColumnGroupRecord("ShortPseudonyms"));
  mImplementor->m.insert(ColumnGroupRecord("CastorShortPseudonyms"));
  mImplementor->m.insert(ColumnGroupRecord("WatchData"));
  mImplementor->m.insert(ColumnGroupRecord("Castor"));
  mImplementor->m.insert(ColumnGroupRecord("Device"));
  mImplementor->m.insert(ColumnGroupRecord("VisitAssessors"));

  // Column with identically-named single column column-group.
  for (const auto& c : {
      "Canary",
      "IsTestParticipant",
      "ParticipantInfo",
      "ParticipantIdentifier",
      "StudyContexts"}) {
    mImplementor->m.insert(ColumnGroupRecord(c));
    mImplementor->m.insert(ColumnRecord(c));
    mImplementor->m.insert(ColumnGroupColumnRecord(c, c));
  }

  // Column group access
  // ///////////////////////////////////////////////////////////////////////////

  // Registration sever
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", "RegistrationServer", "access"));
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", "RegistrationServer", "enumerate"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("ShortPseudonyms", "RegistrationServer", "read"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("ShortPseudonyms", "RegistrationServer", "write"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("ParticipantIdentifier", "RegistrationServer", "read"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("ParticipantIdentifier", "RegistrationServer", "write"));

  // "Pull castor" server
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", "PullCastor", "access"));
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", "PullCastor", "enumerate"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("CastorShortPseudonyms", "PullCastor", "read"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("Castor", "PullCastor", "read"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("Castor", "PullCastor", "write"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("Device", "PullCastor", "read"));

  // Research assessor
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::ResearchAssessor, "access"));
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::ResearchAssessor, "enumerate"));
  for (const auto& cg : {
      "ShortPseudonyms", "WatchData", "Device", "ParticipantIdentifier", "ParticipantInfo", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->m.insert(ColumnGroupAccessRuleRecord(cg, user_group::ResearchAssessor, "read"));
  for (const auto& cg : {"Device", "ParticipantInfo", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->m.insert(ColumnGroupAccessRuleRecord(cg, user_group::ResearchAssessor, "write"));

  // Monitor
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::Monitor, "access"));
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::Monitor, "enumerate"));
  for (const auto& cg : {
      "ShortPseudonyms", "Device", "ParticipantIdentifier", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->m.insert(ColumnGroupAccessRuleRecord(cg, user_group::Monitor, "read"));

  // Data administrator
  // DA has unchecked access to all participant groups: don't grant explicit privileges. See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1923#note_22224
  for (const auto& cg : {"ShortPseudonyms", "WatchData",
      "ParticipantIdentifier", "Device", "Castor", "StudyContexts", "VisitAssessors", "IsTestParticipant" })
    mImplementor->m.insert(ColumnGroupAccessRuleRecord(cg, user_group::DataAdministrator, "read"));

  // Watchdog
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::Watchdog, "access")); // TODO reduce
  mImplementor->m.insert(ParticipantGroupAccessRuleRecord("*", user_group::Watchdog, "enumerate")); // TODO reduce
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("Canary", user_group::Watchdog, "read"));
  mImplementor->m.insert(ColumnGroupAccessRuleRecord("Canary", user_group::Watchdog, "write"));


  LOG(LOG_TAG, info) << "  ... done";
}

std::set<std::string> AccessManager::Backend::Storage::ensureSynced() {
  LOG(LOG_TAG, info) << "Checking whether to create/remove columns ...";
  std::set<std::string> allColumns;
  for (auto& col : this->getColumns(Timestamp())) {
    allColumns.insert(col.name);
  }
  auto ensureColumnExists = [implementor = mImplementor, &allColumns](const std::string& column) {
    if (allColumns.count(column) == 0) {
      LOG(LOG_TAG, warning) << "  adding column " << column;
      allColumns.insert(column);
      implementor->m.insert(ColumnRecord(column));
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
  auto pgars = getParticipantGroupAccessRules(Timestamp(), {.userGroups = std::vector<std::string>{user_group::DataAdministrator}});
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
  auto selectStarPseudonymRecordCountTotal = mImplementor->m.count<SelectStarPseudonymRecord>();
  auto selectStarPseudonymRecordCountOldFormat = mImplementor->m.count<SelectStarPseudonymRecord>(where(length(&SelectStarPseudonymRecord::localPseudonym) > CurvePoint::PACKEDBYTES &&
      length(&SelectStarPseudonymRecord::polymorphicPseudonym) > ElgamalEncryption::PACKEDBYTES));
  auto participantGroupParticipantRecordCountTotal = mImplementor->m.count<ParticipantGroupParticipantRecord>();
  auto participantGroupParticipantRecordCountOldFormat = mImplementor->m.count<ParticipantGroupParticipantRecord>(where(length(&ParticipantGroupParticipantRecord::localPseudonym) > CurvePoint::PACKEDBYTES));

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
    auto transactionGuard = mImplementor->m.transaction_guard();
    for (auto record : mImplementor->m.iterate<SelectStarPseudonymRecord>()) {
      CurvePoint localPseudonymAsPoint = Serialization::FromCharVector<CurvePoint>(record.localPseudonym);
      record.localPseudonym = RangeToVector(localPseudonymAsPoint.pack());
      ElgamalEncryption polymorphicPseudonymAsElgamalEncryption = Serialization::FromCharVector<ElgamalEncryption>(record.polymorphicPseudonym);
      record.polymorphicPseudonym = RangeToVector(polymorphicPseudonymAsElgamalEncryption.pack());
      mImplementor->m.update(record);
    }

    for (auto record : mImplementor->m.iterate<ParticipantGroupParticipantRecord>()) {
      CurvePoint localPseudonymAsPoint = Serialization::FromCharVector<CurvePoint>(record.localPseudonym);
      record.localPseudonym = RangeToVector(localPseudonymAsPoint.pack());
      mImplementor->m.update(record);
    }
    transactionGuard.commit();
    LOG(LOG_TAG, info) << "all records have been updated";
  }
}

void AccessManager::Backend::Storage::removeOrphanedRecords() {
  auto now = Timestamp();

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
  std::set<std::string> groupColumns;
  std::set<std::string> strayColumns;
  std::set<std::string> missingColumns;
  for (auto& record : mImplementor->m.iterate<ColumnGroupColumnRecord>(
        where(c(&ColumnGroupColumnRecord::columnGroup) == columnGroup),
        order_by(&ColumnGroupColumnRecord::timestamp).asc())) {
    if (record.tombstone) groupColumns.erase(record.column);
    else groupColumns.insert(record.column);
  }

  std::set_difference(requiredColumns.cbegin(), requiredColumns.cend(), groupColumns.cbegin(), groupColumns.cend(),
      std::inserter(missingColumns, missingColumns.begin()));
  std::set_difference(groupColumns.cbegin(), groupColumns.cend(), requiredColumns.cbegin(), requiredColumns.cend(),
      std::inserter(strayColumns, strayColumns.begin()));
  for (auto& column : strayColumns) {
    LOG(LOG_TAG, warning) << "  removing column "
      << column << " from column-group " << columnGroup;
    mImplementor->m.insert(ColumnGroupColumnRecord(column, columnGroup, true));
  }
  for (auto& column : missingColumns) {
    LOG(LOG_TAG, warning) << "  adding column " << column << " to column-group " << columnGroup;
    mImplementor->m.insert(ColumnGroupColumnRecord(column, columnGroup));
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
  for (const auto& record : mImplementor->m.iterate<SelectStarPseudonymRecord>())
    mPPCache.emplace(
      record.getLocalPseudonym(),
      record.getPolymorphicPseudonym()
    );

  removeOrphanedRecords();
  LOG(LOG_TAG, info) << "Ready to accept requests!";
}

template<typename T, int version=0>
void computeChecksumImpl(std::shared_ptr<AccessManager::Backend::Storage::Implementor> storage,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint) {
  checkpoint = 1;
  checksum = 0;
  if (!maxCheckpoint)
    maxCheckpoint = std::numeric_limits<int64_t>::max();

  for (const auto& record : storage->m.iterate<T>(
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

// We used to store localPseudonyms and polymorphicPseudonyms as protobufs.
// In order to make sure the conversion went right, we want to make sure there are no checksum chain errors.
// So we add v2 checksums that use the current representation, and convert the local- and polymorphic pseudonyms to the old format for the
// existing checksum. The old version of the checksum can be removed in a later release
const std::unordered_map<std::string, decltype(&computeChecksumImpl<SelectStarPseudonymRecord>)>
    computeChecksumImpls {
  {"select-start-pseud", &computeChecksumImpl<SelectStarPseudonymRecord, 1>},
  {"select-start-pseud-v2", &computeChecksumImpl<SelectStarPseudonymRecord, 2>},
  {"participant-groups", &computeChecksumImpl<ParticipantGroupRecord>},
  {"participant-group-participants", &computeChecksumImpl<ParticipantGroupParticipantRecord, 1>},
  {"participant-group-participants-v2", &computeChecksumImpl<ParticipantGroupParticipantRecord, 2>},
  {"column-groups", &computeChecksumImpl<ColumnGroupRecord>},
  {"columns", &computeChecksumImpl<ColumnRecord>},
  {"column-group-columns", &computeChecksumImpl<ColumnGroupColumnRecord>},
  {"column-group-accessrule", &computeChecksumImpl<ColumnGroupAccessRuleRecord>},
  {"group-accessrule", &computeChecksumImpl<ParticipantGroupAccessRuleRecord>}
};

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
  std::vector<PolymorphicPseudonym> vals;
  vals.reserve(mPPCache.size());
  for (auto& kv : mPPCache)
    vals.push_back(kv.second);
  return vals;
}

std::unordered_map<PolymorphicPseudonym, std::unordered_set<std::string>> AccessManager::Backend::Storage::getPPs(const std::vector<std::string>& participantGroups) {
  /*
  We'd like to combine a JOIN and WHERE clause, but they seem to be incompatible, with
  - JOIN  requiring storage.select(...) and
  - WHERE requiring storage.get_all<>(...).

  auto pps = mImplementor->m.select(columns(&SelectStarPseudonymRecord::polymorphicPseudonym, &ParticipantGroupParticipantRecord::localPseudonym),
    join<ParticipantGroupParticipantRecord>(on(c(&ParticipantGroupParticipantRecord::localPseudonym) == &SelectStarPseudonymRecord::localPseudonym)),
    where(in(&ParticipantGroupParticipantRecord::participantGroup, groups)));

  So as a poor man's solution, we're performing two WHERE queries (using get_all<>) to get the desired selection.
  */
  typedef std::vector<char> charvec;
  typedef std::unordered_set<std::string> stringset;
  std::unordered_map<charvec, stringset, boost::hash<charvec>> localPseudonymsAndGroups;
  localPseudonymsAndGroups.reserve(mPPCache.size());
  auto groupStar = std::find(participantGroups.begin(), participantGroups.end(), "*");
  if (groupStar != participantGroups.end()) {
    for (auto& [lp, _] : mPPCache) {
      localPseudonymsAndGroups.emplace(RangeToVector(lp.pack()), stringset{ "*" });
    }
  }
  for(auto& record : mImplementor->m.iterate<ParticipantGroupParticipantRecord>(where(in(&ParticipantGroupParticipantRecord::participantGroup, participantGroups)
        && c(&ParticipantGroupParticipantRecord::participantGroup) != "*"),
      order_by(&ParticipantGroupParticipantRecord::timestamp))) {
    if(record.tombstone) {
      auto it = localPseudonymsAndGroups.find(record.localPseudonym);
      if (it != localPseudonymsAndGroups.end()) {
        it->second.erase(record.participantGroup);
        if (it->second.empty()) {
          localPseudonymsAndGroups.erase(it);
        }
      }
    } else {
      localPseudonymsAndGroups[record.localPseudonym].insert(record.participantGroup);
    }
  }

  std::vector<charvec> localPseudonyms;
  localPseudonyms.reserve(localPseudonymsAndGroups.size());
  for (auto& [lp, _] : localPseudonymsAndGroups) {
    localPseudonyms.emplace_back(lp);
  }

  auto ssprs = mImplementor->m.get_all<SelectStarPseudonymRecord>(where(in(&SelectStarPseudonymRecord::localPseudonym, localPseudonyms)));
  std::unordered_map<PolymorphicPseudonym, stringset> vals;
  vals.reserve(ssprs.size());
  for (auto& record : ssprs) {
    auto pp = PolymorphicPseudonym::FromPacked(SpanToString(record.polymorphicPseudonym));
    vals.emplace(pp, localPseudonymsAndGroups.at(record.localPseudonym));
  }

  return vals;
}

bool AccessManager::Backend::Storage::hasLocalPseudonym(const LocalPseudonym& localPseudonym) {
  return mPPCache.count(localPseudonym) != 0;
}

void AccessManager::Backend::Storage::storeLocalPseudonymAndPP(
  const LocalPseudonym& localPseudonym,
  const PolymorphicPseudonym& polymorphicPseudonym) {
  auto rerandPolymorphicPseudonym = polymorphicPseudonym.rerandomize();
  mPPCache.emplace(localPseudonym, rerandPolymorphicPseudonym);
  mImplementor->m.insert(SelectStarPseudonymRecord(localPseudonym, rerandPolymorphicPseudonym));
}



/* Core operations on ParticipantGroups */

bool AccessManager::Backend::Storage::hasParticipantGroup(const std::string& name) {
  if (name == "*") { return true; }

  bool ok = false;

  for (auto& record : mImplementor->m.iterate<ParticipantGroupRecord>(
            where(c(&ParticipantGroupRecord::name) == name),
            order_by(&ParticipantGroupRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;

  return ok;
}

std::set<ParticipantGroup> AccessManager::Backend::Storage::getParticipantGroups(const Timestamp& timestamp, const ParticipantGroupFilter& filter) const {
  std::set<ParticipantGroup> result;
  for (auto& record : mImplementor->m.iterate<ParticipantGroupRecord>(
    where(
      c(&ParticipantGroupRecord::timestamp) <= timestamp.getTime() &&
      (!filter.participantGroups.has_value() || in(&ParticipantGroupRecord::name, filter.participantGroups.value_or(emptyVector)))),

    order_by(&ParticipantGroupRecord::timestamp).asc())) {
    ParticipantGroup entry{record.name};
    if (record.tombstone) {
      result.erase(entry);
    }
    else {
      result.insert(entry);
    }
  }
  return result;
}
void AccessManager::Backend::Storage::createParticipantGroup(const std::string& name) {
  if (hasParticipantGroup(name)) {
    std::ostringstream msg;
    msg << "Participant-group " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->m.insert(ParticipantGroupRecord(name));
}

void AccessManager::Backend::Storage::removeParticipantGroup(const std::string& name, const bool force) {
  if (!hasParticipantGroup(name)) {
    std::ostringstream msg;
    msg << "Participant-group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }

  auto guard = mImplementor->m.transaction_guard();

  auto associatedLocalPseudonyms = getParticipantGroupParticipants(Timestamp(), {.participantGroups = std::vector<std::string>{name}});
  auto associatedAccessRules = getParticipantGroupAccessRules(Timestamp(), {.participantGroups = std::vector<std::string>{name}});

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
    std::stringstream msg;
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

  mImplementor->m.insert(ParticipantGroupRecord(name, true));

  guard.commit();
}

/* Core operations on ParticipantGroupParticipants */
bool AccessManager::Backend::Storage::hasParticipantInGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup) {
  bool ok = false;
  for (auto& record : mImplementor->m.iterate<ParticipantGroupParticipantRecord>(
            where(c(&ParticipantGroupParticipantRecord::localPseudonym) == RangeToVector(localPseudonym.pack())
              && c(&ParticipantGroupParticipantRecord::participantGroup) == participantGroup),
            order_by(&ParticipantGroupParticipantRecord::timestamp).desc(),
            limit(1))) {
        ok = !record.tombstone;
  }
  return ok;
}

std::set<ParticipantGroupParticipant> AccessManager::Backend::Storage::getParticipantGroupParticipants(const Timestamp& timestamp, const ParticipantGroupParticipantFilter& filter) const {
  std::set<ParticipantGroupParticipant> result;

  std::vector<std::vector<char>> serializedLocalPseudonyms{}; // create a serialized vector of the LocalPseudonyms for look up.
  if (filter.localPseudonyms.has_value()){
    serializedLocalPseudonyms.reserve(filter.localPseudonyms->size());
    std::transform(filter.localPseudonyms->cbegin(),filter.localPseudonyms->cend(), std::back_inserter(serializedLocalPseudonyms),
                   [](const pep::LocalPseudonym localPseudonym) { return RangeToVector(localPseudonym.pack()); });
        }
  for (auto& record : mImplementor->m.iterate<ParticipantGroupParticipantRecord>(
    where(c(&ParticipantGroupParticipantRecord::timestamp) <= timestamp.getTime() &&

          (!filter.participantGroups.has_value() || in(&ParticipantGroupParticipantRecord::participantGroup, filter.participantGroups.value_or(emptyVector))) &&
          (!filter.localPseudonyms.has_value() || in(&ParticipantGroupParticipantRecord::localPseudonym, serializedLocalPseudonyms))
          ),

    order_by(&ParticipantGroupParticipantRecord::timestamp).asc())) {
    ParticipantGroupParticipant entry(record.participantGroup, record.localPseudonym);
    if (record.tombstone) {
      result.erase(entry);
    }
    else {
      result.insert(entry);
    }
  }
  return result;
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

  auto result = mImplementor->m.select(&SelectStarPseudonymRecord::seqno,
                                   where(c(&SelectStarPseudonymRecord::localPseudonym) == RangeToVector(localPseudonym.pack())),
                                   limit(1));

  if (result.empty()) {
    throw Error("No such participant known");
  }

  mImplementor->m.insert(ParticipantGroupParticipantRecord(localPseudonym, participantGroup));
}

void AccessManager::Backend::Storage::removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup) {
  if (!hasParticipantInGroup(localPseudonym, participantGroup)) {
    // Reporting this error to the data manager allows him to link polymorphic pseudonyms of the same participant. However, he will have this ability anyhow by removing a participant and checking if anything changed (e.g. by performing a list on the participant group)
    std::ostringstream msg;
    msg << "This participant is not part of participant-group " << Logging::Escape(participantGroup);
    throw Error(msg.str());
  }

  mImplementor->m.insert(ParticipantGroupParticipantRecord(localPseudonym, participantGroup, true));
}


/* Core operations on ParticipantGroup Access Rules */
bool AccessManager::Backend::Storage::hasParticipantGroupAccessRule(
    const std::string& participantGroup,
    const std::string& userGroup,
    const std::string& mode) {
  bool ok = false;
  for (auto& record : mImplementor->m.iterate<ParticipantGroupAccessRuleRecord>(
            where(c(&ParticipantGroupAccessRuleRecord::participantGroup) == participantGroup && c(&ParticipantGroupAccessRuleRecord::userGroup) == userGroup && c(&ParticipantGroupAccessRuleRecord::mode) == mode),
            order_by(&ParticipantGroupAccessRuleRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;
  return ok;
}

std::set<ParticipantGroupAccessRule> AccessManager::Backend::Storage::getParticipantGroupAccessRules(const Timestamp& timestamp, const ParticipantGroupAccessRuleFilter& filter) const {
  std::set<ParticipantGroupAccessRule> result;
  for (auto& record : mImplementor->m.iterate<ParticipantGroupAccessRuleRecord>(
    where(c(&ParticipantGroupAccessRuleRecord::timestamp) <= timestamp.getTime() &&

          (!filter.participantGroups.has_value() || in(&ParticipantGroupAccessRuleRecord::participantGroup, filter.participantGroups.value_or(emptyVector))) &&
          (!filter.userGroups.has_value() || in(&ParticipantGroupAccessRuleRecord::userGroup, filter.userGroups.value_or(emptyVector))) &&
          (!filter.modes.has_value() || in(&ParticipantGroupAccessRuleRecord::mode, filter.modes.value_or(emptyVector)))
          ),
    order_by(&ParticipantGroupAccessRuleRecord::timestamp).asc())) {
    ParticipantGroupAccessRule entry(record.participantGroup, record.userGroup, record.mode);
    if (record.tombstone) {
      result.erase(entry);
    }
    else {
      result.insert(entry);
    }
  }
  return result;
}

void AccessManager::Backend::Storage::createParticipantGroupAccessRule(
    const std::string& participantGroup, const std::string& userGroup, const std::string& mode) {
  std::ostringstream msg;

  if (!hasParticipantGroup(participantGroup)) {
        msg << "No such participant-group " << Logging::Escape(participantGroup);
        throw Error(msg.str());
  }
  if (userGroup == user_group::DataAdministrator) {
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

  mImplementor->m.insert(ParticipantGroupAccessRuleRecord(participantGroup, userGroup, mode));
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

  mImplementor->m.insert(ParticipantGroupAccessRuleRecord(participantGroup, userGroup, mode, true));
}


/* Core operations on Columns */
bool AccessManager::Backend::Storage::hasColumn(const std::string& name) {
  bool ok = false;
  for (auto& record : mImplementor->m.iterate<ColumnRecord>(
            where(c(&ColumnRecord::name) == name),
            order_by(&ColumnRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;
  return ok;
}

std::set<Column> AccessManager::Backend::Storage::getColumns(const Timestamp& timestamp, const ColumnFilter& filter) const {
  std::set<Column> result;
  for (auto& record : mImplementor->m.iterate<ColumnRecord>(
    where(c(&ColumnRecord::timestamp) <= timestamp.getTime() &&
          (!filter.columns.has_value() || in(&ColumnRecord::name, filter.columns.value_or(emptyVector)))),
    order_by(&ColumnRecord::timestamp).asc())) {

    Column entry(record.name);
    if (record.tombstone) {
      result.erase({entry});
    }
    else {
      result.insert(entry);
    }
  }
  return result;
}

void AccessManager::Backend::Storage::createColumn(const std::string& name) {
  if (hasColumn(name)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->m.insert(ColumnRecord(name));
  mImplementor->m.insert(ColumnGroupColumnRecord(name, "*"));
}

void AccessManager::Backend::Storage::removeColumn(const std::string& name) {
  if (!hasColumn(name)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }
  auto guard = mImplementor->m.transaction_guard();
  mImplementor->m.insert(ColumnRecord(name, true));
  mImplementor->m.insert(ColumnGroupColumnRecord(name, "*", true));

  //get associated columnGroups
  auto cgcs = getColumnGroupColumns(Timestamp(), {.columns = std::vector<std::string>{name}});
  for (const auto& cgc : cgcs) {
    this->removeColumnFromGroup(cgc.column, cgc.columnGroup);
  }
  guard.commit();
}

bool AccessManager::Backend::Storage::hasColumnGroup(const std::string& name) {
  bool ok = false;
  for (auto& record : mImplementor->m.iterate<ColumnGroupRecord>(
            where(c(&ColumnGroupRecord::name) == name),
            order_by(&ColumnGroupRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;

  return ok;
}

std::set<ColumnGroup> AccessManager::Backend::Storage::getColumnGroups(const Timestamp& timestamp, const ColumnGroupFilter& filter) const {
  std::set<ColumnGroup> result;
  for (auto& record : mImplementor->m.iterate<ColumnGroupRecord>(
    where(c(&ColumnGroupRecord::timestamp) <= timestamp.getTime() &&
          (!filter.columnGroups.has_value() || in(&ColumnGroupRecord::name, filter.columnGroups.value_or(emptyVector)))),
    order_by(&ColumnGroupRecord::timestamp).asc())) {
    ColumnGroup entry(record.name);
    if (record.tombstone) result.erase(entry);
    else result.insert(entry);
  }
  return result;
}

void AccessManager::Backend::Storage::createColumnGroup(const std::string& name) {
  if (hasColumnGroup(name)) {
    std::ostringstream msg;
    msg << "Columngroup " << Logging::Escape(name) << " already exists";
    throw Error(msg.str());
  }
  mImplementor->m.insert(ColumnGroupRecord(name));
}

void AccessManager::Backend::Storage::removeColumnGroup(const std::string& name, const bool force) {
  if (!hasColumnGroup(name)) {
    std::ostringstream msg;
    msg << "Column-group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }
  auto guard = mImplementor->m.transaction_guard();

  auto associatedColumns = getColumnGroupColumns(Timestamp(), {.columnGroups = std::vector<std::string>{name}});
  auto associatedAccessRules = getColumnGroupAccessRules(Timestamp(), {.columnGroups = std::vector<std::string>{name}});

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
    std::stringstream msg;
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
  // If we ended up here, it is safe to remove the columnGroup.
  mImplementor->m.insert(ColumnGroupRecord(name, true));

  guard.commit();
}

/* Core operations on ColumnGroupColumns */
bool AccessManager::Backend::Storage::hasColumnInGroup(
    const std::string& column, const std::string& columnGroup) {
  bool ok = false;

  for (auto& record : mImplementor->m.iterate<ColumnGroupColumnRecord>(
            where(c(&ColumnGroupColumnRecord::column) == column && c(&ColumnGroupColumnRecord::columnGroup) == columnGroup),
            order_by(&ColumnGroupColumnRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;

  return ok;
}

std::set<ColumnGroupColumn> AccessManager::Backend::Storage::getColumnGroupColumns(const Timestamp& timestamp, const ColumnGroupColumnFilter& filter) const {
  std::set<ColumnGroupColumn> result;
  for (auto& record : mImplementor->m.iterate<ColumnGroupColumnRecord>(
    where(c(&ColumnGroupColumnRecord::timestamp) <= timestamp.getTime() &&

      (!filter.columnGroups.has_value() || in(&ColumnGroupColumnRecord::columnGroup, filter.columnGroups.value_or(emptyVector))) &&
      (!filter.columns.has_value() || in(&ColumnGroupColumnRecord::column, filter.columns.value_or(emptyVector)))
      ),
    order_by(&ColumnGroupColumnRecord::timestamp).asc())) {
    ColumnGroupColumn entry(record.columnGroup, record.column);
    if (record.tombstone) {
      result.erase(entry);
    }
    else {
      result.insert(entry);
    }
  }
  return result;
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
  mImplementor->m.insert(ColumnGroupColumnRecord(column, columnGroup));
}

void AccessManager::Backend::Storage::removeColumnFromGroup(
    const std::string& column, const std::string& columnGroup) {
  if (!hasColumnInGroup(column, columnGroup)) {
    std::ostringstream msg;
    msg << "Column " << Logging::Escape(column) << " is not part of "
        << "column-group " << Logging::Escape(columnGroup);
    throw Error(msg.str());
  }
  mImplementor->m.insert(ColumnGroupColumnRecord(column, columnGroup, true));
}

/* Core operations on ColumnGroup Access Rules */
bool AccessManager::Backend::Storage::hasColumnGroupAccessRule(
    const std::string& columnGroup,
    const std::string& userGroup,
    const std::string& mode) {
  bool ok = false;
  for (auto& record : mImplementor->m.iterate<ColumnGroupAccessRuleRecord>(
            where(c(&ColumnGroupAccessRuleRecord::columnGroup) == columnGroup && c(&ColumnGroupAccessRuleRecord::userGroup) == userGroup && c(&ColumnGroupAccessRuleRecord::mode) == mode),
            order_by(&ColumnGroupAccessRuleRecord::timestamp).desc(),
            limit(1)))
        ok = !record.tombstone;
  return ok;
}

std::set<ColumnGroupAccessRule> AccessManager::Backend::Storage::getColumnGroupAccessRules(const Timestamp& timestamp, const ColumnGroupAccessRuleFilter& filter) const {
  std::set<ColumnGroupAccessRule> result;
  for (auto& record : mImplementor->m.iterate<ColumnGroupAccessRuleRecord>(
    where(c(&ColumnGroupAccessRuleRecord::timestamp) <= timestamp.getTime() &&

      (!filter.columnGroups.has_value() || in(&ColumnGroupAccessRuleRecord::columnGroup, filter.columnGroups.value_or(emptyVector))) &&
      (!filter.userGroups.has_value() || in(&ColumnGroupAccessRuleRecord::userGroup, filter.userGroups.value_or(emptyVector))) &&
      (!filter.modes.has_value() || in(&ColumnGroupAccessRuleRecord::mode, filter.modes.value_or(emptyVector)))
      ),
    order_by(&ColumnGroupAccessRuleRecord::timestamp).asc())) {
    ColumnGroupAccessRule entry(record.columnGroup, record.userGroup, record.mode);
    if (record.tombstone) {
      result.erase(entry);
    }
    else {
      result.insert(entry);
    }
  }
  return result;
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
  if (userGroup == user_group::DataAdministrator && mode == "read-meta") {
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

  mImplementor->m.insert(ColumnGroupAccessRuleRecord(
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

  mImplementor->m.insert(ColumnGroupAccessRuleRecord(
      columnGroup, userGroup, mode, true));
}

/* Core operations on Column Name Mappings */
std::vector<ColumnNameMapping> AccessManager::Backend::Storage::getAllColumnNameMappings() const {
  std::vector<ColumnNameMapping> result;
  for (auto& record : mImplementor->m.iterate<ColumnNameMappingRecord>()) {
    result.push_back(record.toLiveObject());
  }
  return result;
}

std::optional<ColumnNameMapping> AccessManager::Backend::Storage::getColumnNameMapping(const ColumnNameSection& original) const {
  // Would have liked to use m.get_no_throw<ColumnNameMappingRecord>(original.getValue()), but I can't get primary key string columns to work
  auto found = mImplementor->m.get_all<ColumnNameMappingRecord>(where((c(&ColumnNameMappingRecord::original) == original.getValue())));
  assert(found.size() < 2);
  if (found.empty()) {
    return std::nullopt;
  }
  return found.front().toLiveObject();
}

void AccessManager::Backend::Storage::createColumnNameMapping(const ColumnNameMapping& mapping) {
  auto record = ColumnNameMappingRecord::FromLiveObject(mapping);
  try {
    mImplementor->m.insert(record);
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
  // Would have liked to use m.update(record), but I can't get primary key string columns to work
  if (!getColumnNameMapping(mapping.original)) {
    throw Error("No mapping found for that original name");
  }
  mImplementor->m.update_all(set(c(&ColumnNameMappingRecord::mapped) = record.mapped),
    where(c(&ColumnNameMappingRecord::original) == record.original));
  assert(getColumnNameMapping(mapping.original).has_value());
  assert(getColumnNameMapping(mapping.original)->original.getValue() == record.original);
  assert(getColumnNameMapping(mapping.original)->mapped.getValue() == record.mapped);
}

void AccessManager::Backend::Storage::deleteColumnNameMapping(const ColumnNameSection& original) {
  // Would have liked to use m.remove<ColumnNameMapping>(original.getValue()), but I can't get primary key string columns to work
  if (!getColumnNameMapping(original)) {
    throw Error("No mapping found for that original name");
  }
  mImplementor->m.remove_all<ColumnNameMappingRecord>(where(c(&ColumnNameMappingRecord::original) == original.getValue()));
  assert(!getColumnNameMapping(original));
}

}
