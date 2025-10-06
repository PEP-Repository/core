#include <pep/accessmanager/UserStorageRecords.hpp>
#include <pep/accessmanager/LegacyAuthserverStorage.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <sstream>

namespace pep {
UserIdRecord::UserIdRecord(
    int64_t internalUserId,
    std::string identifier,
    bool isDisplayId,
    bool tombstone,
    int64_t timestamp) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = timestamp;
  this->internalUserId = internalUserId;
  this->identifier = std::move(identifier);
  this->isDisplayId = isDisplayId;
  this->tombstone = tombstone;
}

uint64_t UserIdRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << internalUserId << '\0' << identifier  << '\0';
  // We only add isDisplayId to the checksum if it is true, because in an earlier version we did not have this field. This way we don't get a checksum change.
  // Probably by mistake, before the addition of isDisplayId, two consecutive `\0`s were written. So that is why the `\0` is not in the conditional part.
  if (isDisplayId)
    os << isDisplayId;
  os << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

UserGroupRecord::UserGroupRecord(int64_t userGroupId, std::string name, std::optional<uint64_t> maxAuthValiditySeconds, bool tombstone) {
  RandomBytes(checksumNonce, 16);
  this->userGroupId = userGroupId;
  this->timestamp = Timestamp().getTime();
  this->name = std::move(name);
  this->tombstone = tombstone;
  this->maxAuthValiditySeconds = maxAuthValiditySeconds;
}

uint64_t UserGroupRecord::checksum() const {
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

UserGroupUserRecord::UserGroupUserRecord(
    int64_t internalUserId,
    int64_t userGroupId,
    bool tombstone) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
  this->internalUserId = internalUserId;
  this->userGroupId = userGroupId;
  this->tombstone = tombstone;
}

UserGroupUserRecord::UserGroupUserRecord(const LegacyUserGroupUserRecord& legacyUserGroupUserRecord)
  : seqno(legacyUserGroupUserRecord.seqno), checksumNonce(legacyUserGroupUserRecord.checksumNonce),
    timestamp(legacyUserGroupUserRecord.timestamp), tombstone(legacyUserGroupUserRecord.tombstone),
    userGroupId(legacyUserGroupUserRecord.userGroupId), internalUserId(legacyUserGroupUserRecord.internalUserId) {}

uint64_t UserGroupUserRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << internalUserId << '\0' << userGroupId << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}
}
