#include <pep/accessmanager/LegacyAuthserverStorage.hpp>

#include <pep/auth/UserGroup.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/accessmanager/UserStorageRecords.hpp>

#include <unordered_map>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>
#include <sqlite_orm/sqlite_orm.h>

namespace pep {

using namespace sqlite_orm;

namespace {
const std::string LOG_TAG("AuthserverStorage");
}

uint64_t LegacyUserGroupUserRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << internalUserId << '\0' << group << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

LegacyAuthserverStorage::LegacyAuthserverStorage(const std::filesystem::path& path) {
  mStorage = std::make_shared<Implementor>(path.string());

  ensureInitialized();
}

// Checks whether the database has been initialized
void LegacyAuthserverStorage::ensureInitialized() {
  LOG(LOG_TAG, info) << "Syncing legacy authserver database schema ...";
  try {
    for(const auto& p : mStorage->raw.sync_schema_simulate(true)) {
      if (p.second == sync_schema_result::dropped_and_recreated)
        throw std::runtime_error("Legacy authserver database schema changed for table " + p.first + ", in a way that would drop the table");
    }
    for(const auto& p : mStorage->raw.sync_schema(true)) {
      if (p.second == sync_schema_result::already_in_sync)
        continue;
      LOG(LOG_TAG, warning) << "  " << p.first << ": " << p.second;
    }
  } catch (const std::system_error& e) {
    LOG(LOG_TAG, error) << "  failed for legacy authserver storage: " << e.what();
    throw;
  }

  if(mStorage->raw.count<UserIdRecord>() == 0) {
    LOG(LOG_TAG, info) << "UserId table empty in legacy authserver storage. Initializing based on existing UserGroupRecords";

    migrateUidToInternalId();
  }
}

int64_t LegacyAuthserverStorage::getNextInternalUserId() const {
  auto currentMax = mStorage->raw.max(&UserIdRecord::internalUserId);
  if (currentMax) {
    return *currentMax + 1;
  }
  return 1;
}

void LegacyAuthserverStorage::migrateUidToInternalId() {
  auto transactionGuard = mStorage->raw.transaction_guard();

  // We're first collecting all records we want to create, so that if we add a
  // tombstone for a user which we encounter again afterwards, we can remove the
  // tombstone altogether from this list, resulting in a cleaner history
  std::list<UserIdRecord> recordsToCreate;
  struct UserInfo {
    int64_t internalId;
    std::unordered_set<std::string> groups;
    std::optional<decltype(recordsToCreate)::iterator> tombstone;
  };
  int64_t nextInternalId = getNextInternalUserId();
  std::unordered_map<std::string /* uid */, UserInfo> knownUsers;
  for (auto record : mStorage->raw.iterate<LegacyUserGroupUserRecord>()) {
    auto knownUser = knownUsers.find(record.uid);
    if (knownUser == knownUsers.end()) {
      // This is the first time we encounter this UID. Add it to UserIds
      int64_t internalId = nextInternalId++;
      std::tie(knownUser, std::ignore) = knownUsers.emplace(
          record.uid, UserInfo{internalId, {}, std::nullopt});
      recordsToCreate.emplace_back(internalId, record.uid, false,
                                   record.timestamp);
    } else if (knownUser->second.tombstone) {
      // We have previously tombstoned this UID, but now we encounter it again.
      // Remove the tombstone
      recordsToCreate.erase(
          *knownUser->second
               .tombstone); // For std::list, this does not invalidate
                            // iterators, which are stored in recordsToCreate
      knownUser->second.tombstone = std::nullopt;
    }
    // Set the internalId on the UserGroupRecord
    record.internalUserId = knownUser->second.internalId;
    mStorage->raw.update(record);
    if (record.tombstone) {
      knownUser->second.groups.erase(record.group);
      if (knownUser->second.groups.empty()) {
        // If there are no groups left of which this UID is a member, tombstone
        // the UID
        knownUser->second.tombstone = recordsToCreate.emplace(
            recordsToCreate.end(), knownUser->second.internalId, record.uid,
            true, record.timestamp);
      }
    } else {
      knownUser->second.groups.insert(record.group);
    }
  }

  mStorage->raw.insert_range(recordsToCreate.begin(), recordsToCreate.end());

  transactionGuard.commit();
}

}
