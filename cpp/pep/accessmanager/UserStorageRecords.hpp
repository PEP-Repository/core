#pragma once

#include <string>
#include <vector>
#include <optional>
#include <pep/crypto/Timestamp.hpp>

namespace pep {

/// A record for an identifier of a user.
/// Users can have multiple known IDs
struct UserIdRecord {
  UserIdRecord() = default;
  UserIdRecord(int64_t internalUserId, std::string identifier, bool isPrimaryId, bool isDisplayId, bool tombstone = false, int64_t timestamp = Timestamp().getTime());
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  /// We use an internal ID to reference a user from other tables, since users identifiers can change, or are not yet known during registration
  int64_t internalUserId{};
  /// The identifier to register or remove for the user
  std::string identifier;
  /// Whether this identifier is the primary identifier for the user
  bool isPrimaryId;
  /// Whether this identifier should be used as the display identifier for the user
  bool isDisplayId;

  static inline const std::tuple RecordIdentifier{
    &UserIdRecord::internalUserId,
    &UserIdRecord::identifier,
  };
};

struct UserGroupRecord {
  UserGroupRecord() = default;
  UserGroupRecord(int64_t userGroupId, std::string name, std::optional<uint64_t> maxAuthValiditySeconds, bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  /// The ID of the user group used internally
  int64_t userGroupId{};
  /// The name of the user group.
  std::string name;
  /// If a user can request long-lived tokens, they can be valid for at most this number of seconds.
  /// nullopt means no long-lived tokens can be requested.
  std::optional<uint64_t> maxAuthValiditySeconds;

  static inline const std::tuple RecordIdentifier{
    &UserGroupRecord::name,
    &UserGroupRecord::userGroupId
  };
};


struct LegacyUserGroupUserRecord;

/**
 * @brief A record for storing user membership of a user group
 */
struct UserGroupUserRecord {
  UserGroupUserRecord() = default;
  UserGroupUserRecord(
    int64_t internalUserId,
    int64_t userGroupId,
    bool tombstone = false);
  explicit UserGroupUserRecord(const LegacyUserGroupUserRecord& legacyUserGroupUserRecord);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  /// The ID of the user group the user is to be a member of. A UserGroupRecord must exist for the group.
  int64_t userGroupId{};
  /// The internalUserId of the user.
  int64_t internalUserId{};

  static inline const std::tuple RecordIdentifier{
    &UserGroupUserRecord::userGroupId,
    &UserGroupUserRecord::internalUserId,
  };
};

}
