#include <pep/accessmanager/ChecksumChain.hpp>
#include <pep/accessmanager/LegacyAuthserverStorage.hpp>
#include <pep/accessmanager/Storage.Implementor.hpp>

namespace pep::accessmanager {

namespace {

using namespace sqlite_orm;

template <typename TRecord>
class AmRecordChecksumChain : public ChecksumChain {
protected:
  virtual Checksum getRecordChecksum(const TRecord& record) const = 0;

public:
  AmRecordChecksumChain(std::string name, AccessManager::Backend::Storage::Implementor& storage) noexcept
    : ChecksumChain(std::move(name), storage) {
  }

  Result get(const std::optional<Checkpoint>& maxCheckpoint) override {
    Result result{
      .checksum = 0,
      .checkpoint = 1,
    };
    auto max = GetMaxSeqNo(maxCheckpoint);

    for (const auto& record : this->storage().raw.template iterate<TRecord>(
      where(c(&TRecord::seqno) <= max))) {
      result.checkpoint = std::max(result.checkpoint, SeqNoToCheckpoint(record.seqno));
      result.checksum ^= this->getRecordChecksum(record);
    }

    return result;
  }
};

template<typename T, int version = 0>
class PlainRecordChecksumChain : public AmRecordChecksumChain<T> {
protected:
  ChecksumChain::Checksum getRecordChecksum(const T& record) const override {
    if constexpr (version == 0) {
      return record.checksum();
    } else {
      return record.checksum(version);
    }
  }

public:
  using AmRecordChecksumChain<T>::AmRecordChecksumChain;
};

//TODO: this checksum is only useful to check the migration for #1642. When that has succeeded, this checksum can be removed in a following release.
class LegacyUserGroupUserChecksumChain : public AmRecordChecksumChain<UserGroupUserRecord> {
protected:
  Checksum getRecordChecksum(const UserGroupUserRecord& record) const override {
    LegacyUserGroupUserRecord legacyRecord(record);

    bool found = false;
    for (auto& record : this->storage().raw.iterate<pep::UserGroupRecord>(
      where(c(&pep::UserGroupRecord::userGroupId) == record.userGroupId
        && c(&pep::UserGroupRecord::timestamp) <= record.timestamp),
      order_by(&pep::UserGroupRecord::seqno).desc(), limit(1))) {
      legacyRecord.group = record.name;
      found = true;
    }
    if (!found)
      throw Error("Could not find user group");

    return legacyRecord.checksum();
  }

public:
  explicit LegacyUserGroupUserChecksumChain(AccessManager::Backend::Storage::Implementor& storage)
    : AmRecordChecksumChain<UserGroupUserRecord>("user-group-users-legacy", storage) {}
};

}

int64_t ChecksumChain::CheckpointToSeqNo(Checkpoint checkpoint) noexcept {
  return static_cast<int64_t>(checkpoint - 2U);
}

ChecksumChain::ChecksumChain(std::string name, AccessManager::Backend::Storage::Implementor& storage) noexcept
  : pep::ChecksumChain(std::move(name)), storage_(storage) {
}

ChecksumChain::Checkpoint ChecksumChain::SeqNoToCheckpoint(int64_t seqno) noexcept {
  return static_cast<Checkpoint>(seqno + 2);
}

int64_t ChecksumChain::GetMaxSeqNo(const std::optional<Checkpoint>& maxCheckpoint) {
  return CheckpointToSeqNo(maxCheckpoint.value_or(std::numeric_limits<int64_t>::max()));
}

ChecksumChains ChecksumChain::CreateAll(AccessManager::Backend::Storage::Implementor& storage) {
  // We used to store localPseudonyms and polymorphicPseudonyms as protobufs.
  // In order to make sure the conversion went right, we want to make sure there are no checksum chain errors.
  // So we add v2 checksums that use the current representation, and convert the local- and polymorphic pseudonyms to the old format for the
  // existing checksum. The old version of the checksum can be removed in a later release
  return ChecksumChains(
    std::make_unique<PlainRecordChecksumChain<SelectStarPseudonymRecord, 1>>("select-start-pseud", storage),
    std::make_unique<PlainRecordChecksumChain<SelectStarPseudonymRecord, 2>>("select-start-pseud-v2", storage),
    std::make_unique<PlainRecordChecksumChain<ParticipantGroupRecord>>("participant-groups", storage),
    std::make_unique<PlainRecordChecksumChain<ParticipantGroupParticipantRecord, 1>>("participant-group-participants", storage),
    std::make_unique<PlainRecordChecksumChain<ParticipantGroupParticipantRecord, 2>>("participant-group-participants-v2", storage),
    std::make_unique<PlainRecordChecksumChain<ColumnGroupRecord>>("column-groups", storage),
    std::make_unique<PlainRecordChecksumChain<ColumnRecord>>("columns", storage),
    std::make_unique<PlainRecordChecksumChain<ColumnGroupColumnRecord>>("column-group-columns", storage),
    std::make_unique<PlainRecordChecksumChain<ColumnGroupAccessRuleRecord>>("column-group-accessrule", storage),
    std::make_unique<PlainRecordChecksumChain<ParticipantGroupAccessRuleRecord>>("group-accessrule", storage),
    std::make_unique<PlainRecordChecksumChain<UserIdRecord>>("user-ids", storage),
    std::make_unique<PlainRecordChecksumChain<UserGroupRecord>>("user-groups", storage),
    std::make_unique<PlainRecordChecksumChain<UserGroupUserRecord>>("user-group-users", storage),
    std::make_unique<LegacyUserGroupUserChecksumChain>(storage),
    std::make_unique<PlainRecordChecksumChain<StructureMetadataRecord>>("structure-metadata", storage)
  );
}

}
