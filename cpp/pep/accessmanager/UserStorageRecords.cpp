#include <pep/accessmanager/UserStorageRecords.hpp>
#include <pep/accessmanager/LegacyAuthserverStorage.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <sstream>

using namespace std::chrono;

namespace pep {
UserIdRecord::UserIdRecord(
    int64_t internalUserId,
    std::string identifier,
    bool tombstone,
    Timestamp timestamp) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = timestamp.ticks_since_epoch<milliseconds>();
  this->internalUserId = internalUserId;
  this->identifier = std::move(identifier);
  this->tombstone = tombstone;
}

uint64_t UserIdRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
     << timestamp << '\0' << internalUserId << '\0' << identifier  << '\0'  << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

UserGroupRecord::UserGroupRecord(int64_t userGroupId, std::string name, std::optional<uint64_t> maxAuthValiditySeconds, bool tombstone) {
  RandomBytes(checksumNonce, 16);
  this->userGroupId = userGroupId;
  this->timestamp = Timestamp::now().ticks_since_epoch<milliseconds>();
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
  this->timestamp = Timestamp::now().ticks_since_epoch<milliseconds>();
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
