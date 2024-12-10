#include <pep/authserver/AuthserverStorage.hpp>

#include <pep/auth/UserGroups.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/Random.hpp>

#include <boost/algorithm/string/join.hpp>
#include <unordered_map>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>

# include <sqlite_orm/sqlite_orm.h>

namespace pep {

using namespace sqlite_orm;

namespace {
  const std::string LOG_TAG("AuthserverStorage");

  using id_pair_set = std::unordered_set<std::pair<int64_t, std::string>,
            boost::hash<std::pair<int64_t, std::string>>>;

  std::optional<std::chrono::seconds> to_optional_seconds(std::optional<uint64_t> val) {
    if(val) {
      return std::chrono::seconds(*val);
    }
    return std::nullopt;
  }

  std::optional<uint64_t> to_optional_uint64(std::optional<std::chrono::seconds> val) {
    if(val) {
      return val->count();
    }
    return std::nullopt;
  }
}

/// A record for an identifier of a user.
/// Users can have multiple known IDs
struct UserIdRecord {
  UserIdRecord() = default;
  UserIdRecord(int64_t internalId, std::string identifier, bool tombstone = false, int64_t timestamp = Timestamp().getTime());
  uint64_t checksum() const;

  int64_t seqno;
  std::vector<char> checksumNonce;
  int64_t timestamp;
  bool tombstone;

  /// We use an internal ID to reference a user from other tables, since users identifiers can change, or are not yet known during registration
  int64_t internalId;
  /// The identifier to register or remove for the user
  std::string identifier;
};

struct GroupRecord {
  GroupRecord() = default;
  GroupRecord(std::string name, std::optional<uint64_t> maxAuthValiditySeconds, bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno;
  std::vector<char> checksumNonce;
  int64_t timestamp;
  bool tombstone;

  /// The name of the user group.
  std::string name;
  /// If a user can request long-lived tokens, they can be valid for at most this number of seconds.
  /// nullopt means no long-lived tokens can be requested.
  std::optional<uint64_t> maxAuthValiditySeconds;
};

/**
 * @brief A record for storing user membership of a user group
 */
struct UserGroupRecord {
  UserGroupRecord() = default;
  UserGroupRecord(
    int64_t internalId,
    std::string group,
    bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno;
  std::vector<char> checksumNonce;
  int64_t timestamp;
  bool tombstone;

  /// The user group the user is to be a member of. A GroupRecord must exist for the group.
  std::string group;
  /// The uid of the user. Deprecated. Only used for migration
  std::string uid;
  /// The internalId of the user.
  int64_t internalId;
};

// This function defines the database scheme used.
inline auto authserver_create_db(const std::string& path) {
  return make_storage(path,
    make_index("idx_UserIds",
      &UserIdRecord::internalId,
      &UserIdRecord::identifier,
      &UserIdRecord::timestamp),
    make_table("UserIds",
      make_column("seqno",
        &UserIdRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserIdRecord::checksumNonce),
      make_column("timestamp", &UserIdRecord::timestamp),
      make_column("tombstone", &UserIdRecord::tombstone),
      make_column("internalId", &UserIdRecord::internalId),
      make_column("identifier", &UserIdRecord::identifier)),

    make_index("idx_Groups",
      &GroupRecord::name,
      &GroupRecord::timestamp),
    make_table("Groups",
      make_column("seqno",
        &GroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &GroupRecord::checksumNonce),
      make_column("timestamp", &GroupRecord::timestamp),
      make_column("tombstone", &GroupRecord::tombstone),
      make_column("name", &GroupRecord::name),
      make_column("maxAuthValiditySeconds", &GroupRecord::maxAuthValiditySeconds)),

    make_index("idx_UserGroups",
      &UserGroupRecord::uid,
      &UserGroupRecord::group,
      &UserGroupRecord::timestamp),
    make_table("UserGroups",
      make_column("seqno",
        &UserGroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserGroupRecord::checksumNonce),
      make_column("timestamp", &UserGroupRecord::timestamp),
      make_column("tombstone", &UserGroupRecord::tombstone),
      make_column("uid", &UserGroupRecord::uid), //deprecated, only used for migration to internalId. Can be removed when migration is no longer needed (i.e. all environments have been migrated)
      make_column("internalId", &UserGroupRecord::internalId, default_value(-1)),
      make_column("group", &UserGroupRecord::group))
  );
}

UserIdRecord::UserIdRecord(
    int64_t internalId,
    std::string identifier,
    bool tombstone,
    int64_t timestamp) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = timestamp;
  this->internalId = internalId;
  this->identifier = std::move(identifier);
  this->tombstone = tombstone;
}

uint64_t UserIdRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << internalId << '\0' << identifier  << '\0'  << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

GroupRecord::GroupRecord(std::string name, std::optional<uint64_t> maxAuthValiditySeconds, bool tombstone) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
  this->name = std::move(name);
  this->tombstone = tombstone;
  this->maxAuthValiditySeconds = maxAuthValiditySeconds;
}

uint64_t GroupRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << name;

  // Don't include maxAuthValiditySeconds in calculation if it's not set, to ensure that we calculate a backward
  // compatible value for old records. See https://gitlab.pep.cs.ru.nl/pep/ops/-/issues/183#note_33937
  if (maxAuthValiditySeconds) {
    os << '\0' << (*maxAuthValiditySeconds + 1);
  }

  os << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

UserGroupRecord::UserGroupRecord(
    int64_t internalId,
    std::string group,
    bool tombstone) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
  this->internalId = std::move(internalId);
  this->group = std::move(group);
  this->tombstone = tombstone;
}

uint64_t UserGroupRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << uid << '\0' << group << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

class AuthserverStorageBackend {
public:
  AuthserverStorageBackend(const std::string& path)
    : m(authserver_create_db(path)) { }
  decltype(authserver_create_db("")) m;
};

AuthserverStorage::AuthserverStorage(const std::filesystem::path& path) {
  mStorage = std::make_shared<AuthserverStorageBackend>(path.string());

  ensureInitialized();
}

// Checks whether the database has been initialized
void AuthserverStorage::ensureInitialized() {
  LOG(LOG_TAG, info) << "Synching database schema ...";
  try {
    for(const auto& p : mStorage->m.sync_schema_simulate(true)) {
      if (p.second == sync_schema_result::dropped_and_recreated)
        throw std::runtime_error("Database schema changed for table " + p.first + ", in a way that would drop the table");
    }
    for(const auto& p : mStorage->m.sync_schema(true)) {
      if (p.second == sync_schema_result::already_in_sync)
        continue;
      LOG(LOG_TAG, warning) << "  " << p.first << ": " << p.second;
    }
  } catch (const std::system_error& e) {
    LOG(LOG_TAG, error) << "  failed: " << e.what();
    throw;
  }

  #ifdef ENABLE_OAUTH_TEST_USERS
  if (mStorage->m.count<GroupRecord>() == 0) {
    LOG(LOG_TAG, warning) << "Database seems uninitialized.  Initializing ...";

    // For testing purposes, we want some users which can request long-lived tokens, and some that can't.
    // I chose data admin as the role that can get long-lived tokens, but it could have been any user/group entry
    createGroup(user_group::ResearchAssessor, UserGroupProperties());
    createGroup(user_group::Monitor, UserGroupProperties());
    createGroup(user_group::DataAdministrator, UserGroupProperties(std::chrono::hours(24)));
    createGroup(user_group::AccessAdministrator, UserGroupProperties());

    auto assessorId = createUser("assessor@master.pep.cs.ru.nl");
    auto monitorId = createUser("monitor@master.pep.cs.ru.nl");
    auto dataadminId = createUser("dataadmin@master.pep.cs.ru.nl");
    auto accessadminId = createUser("accessadmin@master.pep.cs.ru.nl");
    auto multihatId = createUser("multihat@master.pep.cs.ru.nl");

    addUserToGroup(assessorId, user_group::ResearchAssessor);
    addUserToGroup(monitorId, user_group::Monitor);
    addUserToGroup(dataadminId, user_group::DataAdministrator);
    addUserToGroup(accessadminId, user_group::AccessAdministrator);

    addUserToGroup(multihatId, user_group::ResearchAssessor);
    addUserToGroup(multihatId, user_group::Monitor);
    addUserToGroup(multihatId, user_group::DataAdministrator);
    addUserToGroup(multihatId, user_group::AccessAdministrator);
  }

  LOG(LOG_TAG, warning) << "  ... done";
#endif //ENABLE_OAUTH_TEST_USERS

  if(mStorage->m.count<UserIdRecord>() == 0) {
    LOG(LOG_TAG, info) << "UserId table empty. Initializing based on existing UserGroupRecords";

    migrateUidToInternalId();
  }
}

void AuthserverStorage::migrateUidToInternalId() {
  auto transactionGuard = mStorage->m.transaction_guard();

  // We're first collecting all records we want to create, so that if we add a tombstone for a user which we encounter
  // again afterwards, we can remove the tombstone altogether from this list, resulting in a cleaner history
  std::list<UserIdRecord> recordsToCreate;
  struct UserInfo {
    int64_t internalId;
    std::unordered_set<std::string> groups;
    std::optional<decltype(recordsToCreate)::iterator> tombstone;
  };
  int64_t nextInternalId = getNextInternalId();
  std::unordered_map<std::string /* uid */, UserInfo> knownUsers;
  for (auto record : mStorage->m.iterate<UserGroupRecord>()) {
    auto knownUser = knownUsers.find(record.uid);
    if (knownUser == knownUsers.end()) {
      // This is the first time we encounter this UID. Add it to UserIds
      int64_t internalId = nextInternalId++;
      std::tie(knownUser, std::ignore) = knownUsers.emplace(record.uid, UserInfo{internalId, {}, std::nullopt});
      recordsToCreate.emplace_back(internalId, record.uid, false, record.timestamp);
    }
    else if(knownUser->second.tombstone) {
      // We have previously tombstoned this UID, but now we encounter it again. Remove the tombstone
      recordsToCreate.erase(*knownUser->second.tombstone); // For std::list, this does not invalidate iterators, which are stored in recordsToCreate
      knownUser->second.tombstone = std::nullopt;
    }
    // Set the internalId on the UserGroupRecord
    record.internalId = knownUser->second.internalId;
    mStorage->m.update(record);
    if (record.tombstone) {
      knownUser->second.groups.erase(record.group);
      if (knownUser->second.groups.empty()) {
        // If there are no groups left of which this UID is a member, tombstone the UID
        knownUser->second.tombstone =
          recordsToCreate.emplace(recordsToCreate.end(), knownUser->second.internalId, record.uid, true, record.timestamp);

      }
    }
    else {
      knownUser->second.groups.insert(record.group);
    }
  }

  mStorage->m.insert_range(recordsToCreate.begin(), recordsToCreate.end());

  transactionGuard.commit();
}

int64_t AuthserverStorage::getNextInternalId() const {
  auto currentMax = mStorage->m.max(&UserIdRecord::internalId);
  if (currentMax) {
    return *currentMax + 1;
  }
  return 1;
}

int64_t AuthserverStorage::createUser(std::string identifier) {
  int64_t internalId = getNextInternalId();
  addIdentifierForUser(internalId, std::move(identifier));
  return internalId;
}

void AuthserverStorage::removeUser(std::string_view uid) {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  removeUser(*internalId);
}

void AuthserverStorage::removeUser(int64_t internalId) {
  auto groups = getUserGroups(internalId);
  if(!groups.empty()) {
    if(groups.size() > 10)
      throw Error("User is still in " + std::to_string(groups.size()) + " user groups");
    throw Error("User is still in user groups: " + boost::algorithm::join(groups, ", "));
  }

  for(auto& uid : getAllIdentifiersForUser(internalId))
    mStorage->m.insert(UserIdRecord(internalId, uid, true));
}

void AuthserverStorage::addIdentifierForUser(std::string_view uid, std::string identifier) {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  addIdentifierForUser(*internalId, identifier);
}

void AuthserverStorage::addIdentifierForUser(int64_t internalId, std::string identifier) {
  // Regardless of the value of humanReadable, we don't want the identifier to already exist as
  // either a system ID or a human-readable ID.
  if (findInternalId(identifier)) {
    throw Error("The user identifier already exists");
  }
  mStorage->m.insert(UserIdRecord(internalId, std::move(identifier)));
}

void AuthserverStorage::removeIdentifierForUser(std::string identifier) {
  std::optional<int64_t> internalId = findInternalId(identifier);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  removeIdentifierForUser(*internalId, std::move(identifier));
}

void AuthserverStorage::removeIdentifierForUser(int64_t internalId, std::string identifier) {
  auto identifiers = getAllIdentifiersForUser(internalId);
  if(identifiers.size() == 0)
    throw Error("The user does not exist");
  if(identifiers.size() == 1)
    throw Error("You are trying to remove the last identifier for a user. This will make it impossible to "
      "address that user, and is therefore not allowed. Instead, you can remove the user, if that is the intention");

  if(!identifiers.contains(identifier)) {
    throw Error("The given identifier does not exist for the given internalId");
  }

  mStorage->m.insert(UserIdRecord(internalId, std::move(identifier), true));
}

std::optional<int64_t> AuthserverStorage::findInternalId(std::string_view identifier, Timestamp at) const {
  for (auto& record : mStorage->m.iterate<UserIdRecord>(
    where(
      c(&UserIdRecord::identifier) == identifier
      && c(&UserIdRecord::timestamp) <= at.getTime()),
    order_by(&UserIdRecord::timestamp).desc(),
    limit(1)
  )) {
    if(record.tombstone) return std::nullopt;
    return record.internalId;
  }
  return std::nullopt;
}

std::optional<int64_t> AuthserverStorage::findInternalId(const std::vector<std::string>& identifiers, Timestamp at) const {
  std::unordered_map<std::string, int64_t> foundIdentifiers;
  for (auto& record : mStorage->m.iterate<UserIdRecord>(
    where(
      in(&UserIdRecord::identifier, identifiers)
      && c(&UserIdRecord::timestamp) <= at.getTime()),
    order_by(&UserIdRecord::timestamp).asc(),
    limit(1)
  )) {
    if(record.tombstone) {
      assert(foundIdentifiers.contains(record.identifier));
      assert(foundIdentifiers.at(record.identifier) == record.internalId);
      foundIdentifiers.erase(record.identifier);
    }
    else {
      foundIdentifiers.emplace(record.identifier, record.internalId);
    }
  }
  if(foundIdentifiers.empty())
    return std::nullopt;

#ifndef NDEBUG
  for(auto& identifier : foundIdentifiers) {
    assert(identifier.second == foundIdentifiers.begin()->second);
  }
#endif

  return foundIdentifiers.begin()->second;
}

std::unordered_set<std::string> AuthserverStorage::getAllIdentifiersForUser(int64_t internalId, Timestamp at) const {
  std::unordered_set<std::string> identifiers;
  for(auto& record : mStorage->m.iterate<UserIdRecord>(
        where(c(&UserIdRecord::internalId) == internalId
              && c(&UserIdRecord::timestamp) <= at.getTime()),
        order_by(&UserIdRecord::timestamp).asc())) {
    if (record.tombstone) identifiers.erase(record.identifier);
    else identifiers.insert(record.identifier);
  }
  return identifiers;
}

std::unordered_set<std::string> AuthserverStorage::getUserGroups(std::string_view uid, Timestamp at) const {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  return getUserGroups(*internalId, at);
}


std::unordered_set<std::string> AuthserverStorage::getUserGroups(int64_t internalId, Timestamp at) const {
  std::unordered_set<std::string> groups;
  for(auto& record : mStorage->m.iterate<UserGroupRecord>(
        where(c(&UserGroupRecord::internalId) == internalId
              && c(&UserGroupRecord::timestamp) <= at.getTime()),
        order_by(&UserGroupRecord::timestamp).asc())) {
    if (record.tombstone) groups.erase(record.group);
    else groups.insert(record.group);
  }
  return groups;
}

bool AuthserverStorage::hasGroup(std::string_view name) const {
  bool ok = false;

  for (auto& record : mStorage->m.iterate<GroupRecord>(
      where(c(&GroupRecord::name) == name),
      order_by(&GroupRecord::timestamp).desc(),
      limit(1)))
    ok = !record.tombstone;

  return ok;
}

std::optional<std::chrono::seconds> AuthserverStorage::getMaxAuthValidity(const std::string& group) const {
  for (auto& record : mStorage->m.iterate<GroupRecord>(
    where(c(&GroupRecord::name) == group),
      order_by(&GroupRecord::timestamp).desc(),
      limit(1))) {
    if(!record.tombstone) {
      if(record.maxAuthValiditySeconds) {
        return std::chrono::seconds(*record.maxAuthValiditySeconds);
      }
      else {
        return std::nullopt;
      }
    }
  }
  std::ostringstream msg;
  msg << "Could not find group " << Logging::Escape(group);
  throw Error(msg.str());
}

bool AuthserverStorage::userInGroup(std::string_view uid, std::string_view group) const {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  return userInGroup(*internalId, group);
}

bool AuthserverStorage::userInGroup(int64_t internalId, std::string_view group) const {
  bool ok = false;
  for (auto& record : mStorage->m.iterate<UserGroupRecord>(
    where(c(&UserGroupRecord::internalId) == internalId
        && c(&UserGroupRecord::group) == group),
      order_by(&UserGroupRecord::timestamp).desc(),
      limit(1))) {
    ok = !record.tombstone;
  }

  return ok;
}

void AuthserverStorage::modifyOrCreateGroup(std::string name,
                                            const UserGroupProperties& properties,
                                            bool create) {
  if (hasGroup(name) == create) {
    std::ostringstream msg;
    msg << "group " << Logging::Escape(name);
    if (create)
      msg << " already exists";
    else
      msg << " doesn't exist";
    throw Error(msg.str());
  }

  mStorage->m.insert(GroupRecord(std::move(name), to_optional_uint64(properties.mMaxAuthValidity)));
}

void AuthserverStorage::createGroup(std::string name, const UserGroupProperties& properties) {
  modifyOrCreateGroup(std::move(name), properties, true);
}

void AuthserverStorage::modifyGroup(std::string name, const UserGroupProperties& properties) {
  modifyOrCreateGroup(std::move(name), properties, false);
}

void AuthserverStorage::removeGroup(std::string name) {
  if (!hasGroup(name)) {
    std::ostringstream msg;
    msg << "group " << Logging::Escape(name) << " does not exist";
    throw Error(msg.str());
  }

  std::unordered_set<std::string> uids;
  for(auto& record : mStorage->m.iterate<UserGroupRecord>(
      where(c(&UserGroupRecord::group) == name),
      order_by(&UserGroupRecord::timestamp))) {
    if(record.tombstone) {
      uids.erase(record.group);
    }
    else  {
      uids.insert(record.group);
    }
  }
  if(!uids.empty()) {
    std::ostringstream msg;
    msg << "Group " << Logging::Escape(name) << " still has users. Group will not be removed";
    throw Error(msg.str());
  }

  mStorage->m.insert(GroupRecord(name, std::nullopt, true));
}

void AuthserverStorage::addUserToGroup(std::string_view uid, std::string group) {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  addUserToGroup(*internalId, std::move(group));
}

void AuthserverStorage::addUserToGroup(int64_t internalId, std::string group) {
  std::ostringstream msg;
  if (userInGroup(internalId, group)) {
    msg << "User is already in group: " << Logging::Escape(group);
    throw Error(msg.str());
  }

  if (!hasGroup(group)) {
    msg << "No such group: " << Logging::Escape(group);
    throw Error(msg.str());
  }

  mStorage->m.insert(UserGroupRecord(internalId, std::move(group)));
}

void AuthserverStorage::removeUserFromGroup(std::string_view uid, std::string group) {
  std::optional<int64_t> internalId = findInternalId(uid);
  if(!internalId) {
    throw Error("Could not find user id");
  }
  removeUserFromGroup(*internalId, std::move(group));
}

void AuthserverStorage::removeUserFromGroup(int64_t internalId, std::string group) {
  if (!userInGroup(internalId, group)) {
    std::ostringstream msg;
    msg << "This user is not part of group " << Logging::Escape(group);
    throw Error(msg.str());
  }

  mStorage->m.insert(UserGroupRecord(internalId, std::move(group), true));
}

AsaQueryResponse AuthserverStorage::executeQuery(const AsaQuery& query) {
  AsaQueryResponse ret;

  std::unordered_map<int64_t, std::unordered_set<std::string>> filteredIds;
  for (auto& record : mStorage->m.iterate<UserIdRecord>(where(c(&UserIdRecord::timestamp) <= query.mAt.getTime()),
                                                        order_by(&UserIdRecord::timestamp).asc())) {
    if (!query.mUserFilter.empty() && !boost::contains(record.identifier, query.mUserFilter))
      continue;
    if (record.tombstone) {
      auto entry = filteredIds.find(record.internalId);
      if (entry == filteredIds.end()) {
        LOG(LOG_TAG, warning) << "User identifier '" << record.identifier << "' for internalId " << record.internalId
                              << " is tombstoned, but that internalId was not previously encountered.";
      }
      else {
        entry->second.erase(record.identifier);
      }
    }
    else {
      filteredIds[record.internalId].insert(record.identifier);
    }
  }

  std::unordered_set<int64_t> filteredInternalIds;
  for(auto& [internalId, identifiers] : filteredIds) {
    if(!identifiers.empty()) {
      filteredInternalIds.insert(internalId);
    }
  }

  // Make a set containing all the associations between a user and a usergroup
  id_pair_set userToGroupAssociationSet;
  for (auto& record : mStorage->m.iterate<UserGroupRecord>(
        where(c(&UserGroupRecord::timestamp) <= query.mAt.getTime()),
        order_by(&UserGroupRecord::timestamp).asc())) {
    if (!query.mUserFilter.empty()
          && !filteredInternalIds.contains(record.internalId))
      continue;
    if (!query.mGroupFilter.empty()
          && !boost::contains(record.group, query.mGroupFilter))
      continue;
    auto pair = std::make_pair(record.internalId, record.group);
    if (record.tombstone) userToGroupAssociationSet.erase(pair);
    else userToGroupAssociationSet.insert(pair);
  }

  // Make a map that only contains users that match the userFilter, mapping from internalId to AsaQRUser.
  std::map<int64_t, AsaQRUser> usersMap;
  // If there is no groupFilter, we want to make sure that all users are in the usersMap, so also the ones not in any groups
  if(query.mGroupFilter.empty()) {
    for(int64_t internalId : filteredInternalIds) {
      usersMap.emplace(internalId, AsaQRUser());
    }
  }
  // Add the groups to the users in the map, while creating new entries if groupFilter is not empty, and they are not yet created.
  for (auto& [internalId, group] : userToGroupAssociationSet) {
    usersMap[internalId].mGroups.push_back(group);
  }
  // Construct final user list, while adding all known identifiers to the users
  for (auto& [internalId, user] : usersMap) {
    // If userFilter is not empty, it is possible that the user wants to see other, non-matching uids as well for matching users.
    // So we won't use the ids in filteredIds, but query the storage for all identifiers for the user.
    auto identifiers = getAllIdentifiersForUser(internalId, query.mAt);
    for(auto& identifier : identifiers) {
      user.mUids.push_back(std::move(identifier));
    }
    ret.mUsers.push_back(std::move(user));
  }

  // Make a map that only contains groups that match the filters, mapping to their properties
  std::map<std::string, UserGroupProperties> groupsWithProperties;
  if(!query.mUserFilter.empty()) {
    // If userFilter is not empty, we only want to show groups of users matching the userfilter. So we loop over the
    // userToGroupAssociationSet that was filled based on userFilter and groupfilter.
    for (auto& [uid, group] : userToGroupAssociationSet) {
      auto [it, inserted] = groupsWithProperties.try_emplace(group);
      if(inserted) {
        it->second.mMaxAuthValidity = getMaxAuthValidity(group);
      }
    }
  }
  else {
    // If userFilter is empty, we want to see all groups. So also the ones that don't have any users. Therefore, we query the
    // storage to get all the groups.
    for (auto& record : mStorage->m.iterate<GroupRecord>(
          where(c(&GroupRecord::timestamp) <= query.mAt.getTime()),
          order_by(&GroupRecord::timestamp).asc())) {
      if (!query.mGroupFilter.empty()
            && !boost::contains(record.name, query.mGroupFilter))
        continue;
      if (record.tombstone) groupsWithProperties.erase(record.name);
      else groupsWithProperties.insert_or_assign(record.name, UserGroupProperties(to_optional_seconds(record.maxAuthValiditySeconds)));
    }
  }

  // Construct final group list
  ret.mUserGroups.reserve(groupsWithProperties.size());
  for (auto& [group, properties] : groupsWithProperties) {
      ret.mUserGroups.push_back(AsaQRUserGroup(group, std::move(properties)));
  }

  return ret;
}

template<typename T>
void computeChecksumImpl(std::shared_ptr<AuthserverStorageBackend> storage,
    std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint) {
  checkpoint = 1;
  checksum = 0;
  if (!maxCheckpoint)
    maxCheckpoint = std::numeric_limits<int64_t>::max();

  for (const auto& record : storage->m.iterate<T>(
      where(c(&T::seqno) < static_cast<int64_t>(*maxCheckpoint) - 1))) {
    checkpoint = std::max(checkpoint, static_cast<uint64_t>(record.seqno + 2));
    checksum ^= record.checksum();
  }
}

const std::unordered_map<
  std::string, decltype(&computeChecksumImpl<GroupRecord>)>
    computeChecksumImpls {
  {"groups", &computeChecksumImpl<GroupRecord>},
  {"user-groups", &computeChecksumImpl<UserGroupRecord>}
};

void AuthserverStorage::computeChecksum(const std::string& chain,
      std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
      uint64_t& checkpoint) {
  if (computeChecksumImpls.count(chain) == 0)
    throw Error("No such checksum chain");
  computeChecksumImpls.at(chain)(mStorage, maxCheckpoint, checksum, checkpoint);
}

std::vector<std::string> AuthserverStorage::getChecksumChainNames() {
  std::vector<std::string> ret;
  ret.reserve(computeChecksumImpls.size());
  for (const auto& pair : computeChecksumImpls)
    ret.push_back(pair.first);
  return  ret;
}

}
