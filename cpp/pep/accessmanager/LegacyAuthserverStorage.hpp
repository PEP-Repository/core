#pragma once

#include <filesystem>
#include <pep/accessmanager/Backend.hpp>
#include <pep/accessmanager/UserStorageRecords.hpp>
#include <pep/database/Storage.hpp>

namespace pep {

struct LegacyUserGroupUserRecord  {
  LegacyUserGroupUserRecord() = default;
  LegacyUserGroupUserRecord(
    int64_t internalUserId,
    int64_t userGroupId,
    bool tombstone = false) : tombstone(tombstone), userGroupId(userGroupId), internalUserId(internalUserId) {}
  explicit LegacyUserGroupUserRecord(const UserGroupUserRecord& userGroupUserRecord)
    : seqno(userGroupUserRecord.seqno), checksumNonce(userGroupUserRecord.checksumNonce), timestamp(userGroupUserRecord.timestamp),
      tombstone(userGroupUserRecord.tombstone), userGroupId(userGroupUserRecord.userGroupId), internalUserId(userGroupUserRecord.internalUserId) {}
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  database::UnixMillis timestamp{};
  bool tombstone{};

  /// The ID of the user group the user is to be a member of. A UserGroupRecord must exist for the group.
  int64_t userGroupId{};
  /// The internalUserId of the user.
  int64_t internalUserId{};

  /// Deprecated. Only used for migration
  std::string group;
  /// The uid of the user. Deprecated. Only used for migration
  std::string uid;
};

namespace detail {
// This function defines the database scheme used.
static auto legacy_authserver_create_db(const std::string& path) {
  using namespace sqlite_orm;
  // This is very similar to how the same tables are defined in AccessManagerStorage, but not entirely the same.
  // Some refactorings have been done during the migration of these tables to the access manager. So here we have the
  // old database structure, while we have the new structure in AccessManagerStorage.
  return make_storage(path,
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
      make_column("internalId", &UserIdRecord::internalUserId),
      make_column("identifier", &UserIdRecord::identifier)),

    make_index("idx_Groups",
      &UserGroupRecord::name,
      &UserGroupRecord::timestamp),
    make_table("Groups",// UserGroupRecords were previously called just `GroupRecord` in the authserver code. This has been refactored, but we can't easily rename the table in existing production DBs
      make_column("seqno",
        &UserGroupRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &UserGroupRecord::checksumNonce),
      make_column("timestamp", &UserGroupRecord::timestamp),
      make_column("tombstone", &UserGroupRecord::tombstone),
      make_column("name", &UserGroupRecord::name),
      make_column("maxAuthValiditySeconds", &UserGroupRecord::maxAuthValiditySeconds)),

    make_index("idx_UserGroups",
      &LegacyUserGroupUserRecord::uid,
      &LegacyUserGroupUserRecord::group,
      &LegacyUserGroupUserRecord::timestamp),
    make_table("UserGroups", // UserGroupUserRecords were previously called just `UserGroupRecord` in the authserver code. This has been refactored, but we can't easily rename the table in existing production DBs
      make_column("seqno",
        &LegacyUserGroupUserRecord::seqno,
        primary_key().autoincrement()),
      make_column("checksumNonce", &LegacyUserGroupUserRecord::checksumNonce),
      make_column("timestamp", &LegacyUserGroupUserRecord::timestamp),
      make_column("tombstone", &LegacyUserGroupUserRecord::tombstone),
      make_column("uid", &LegacyUserGroupUserRecord::uid), //deprecated, only used for migration to internalId. Can be removed when migration is no longer needed (i.e. all environments have been migrated)
      make_column("internalId", &LegacyUserGroupUserRecord::internalUserId, default_value(-1)),
      make_column("group", &LegacyUserGroupUserRecord::group))
  );
}
}

class LegacyAuthserverStorage {
private:
  using Implementor = database::Storage<detail::legacy_authserver_create_db>;

  std::shared_ptr<Implementor> mStorage;

  int64_t getNextInternalUserId() const;

  void ensureInitialized();
  void migrateUidToInternalId();

public:
  LegacyAuthserverStorage(const std::filesystem::path &path);

  auto getUserIdRecords() const {
    return mStorage->raw.iterate<UserIdRecord>(sqlite_orm::order_by(&UserIdRecord::seqno).asc());
  };

  auto getUserGroupRecords() const{
    return mStorage->raw.iterate<UserGroupRecord>(sqlite_orm::order_by(&UserGroupRecord::seqno).asc());
  };

  auto getUserGroupUserRecords() const{
    return mStorage->raw.iterate<LegacyUserGroupUserRecord>(sqlite_orm::order_by(&LegacyUserGroupUserRecord::seqno).asc());
  };
};

}
