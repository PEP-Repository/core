#include <pep/accessmanager/Records.hpp>

#include <pep/crypto/Timestamp.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Sha.hpp>

#include <sqlite_orm/sqlite_orm.h>
#include <string_view>
#include <type_traits>

namespace pep {

SelectStarPseudonymRecord::SelectStarPseudonymRecord(LocalPseudonym lp, PolymorphicPseudonym pp) {
  localPseudonym = RangeToVector(lp.pack());
  polymorphicPseudonym = RangeToVector(pp.pack());
}

uint64_t SelectStarPseudonymRecord::checksum(int version) const {
  Sha256 hasher;
  switch (version) {
  case 1: { // Old
    CurvePoint localPseudonymAsCurvePoint(SpanToString(localPseudonym));
    hasher.update(Serialization::ToString<CurvePoint>(localPseudonymAsCurvePoint));

    ElgamalEncryption polymorphicPseudonymAsElgamalEncryption =
        ElgamalEncryption::FromPacked(SpanToString(polymorphicPseudonym));
    hasher.update(Serialization::ToString<ElgamalEncryption>(polymorphicPseudonymAsElgamalEncryption));
    break;
  }
  default: // New
    hasher
      .update(SpanToString(localPseudonym))
      .update(SpanToString(polymorphicPseudonym));
    break;
  }
  return UnpackUint64BE(hasher.digest());
}

LocalPseudonym SelectStarPseudonymRecord::getLocalPseudonym() const {
  return LocalPseudonym::FromPacked(SpanToString(localPseudonym));
}

PolymorphicPseudonym SelectStarPseudonymRecord::getPolymorphicPseudonym() const {
  return PolymorphicPseudonym::FromPacked(SpanToString(polymorphicPseudonym));
}

ParticipantGroupRecord::ParticipantGroupRecord(std::string name, bool tombstone) : tombstone(tombstone), name(std::move(name)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ParticipantGroupRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << name << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

ParticipantGroupParticipantRecord::ParticipantGroupParticipantRecord(
  LocalPseudonym localPseudonym,
  std::string participantGroup,
  bool tombstone) : tombstone(tombstone), participantGroup(std::move(participantGroup)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
  this->localPseudonym = RangeToVector(localPseudonym.pack());
}

uint64_t ParticipantGroupParticipantRecord::checksum(int version) const {
  std::string localPseudonymString(SpanToString(localPseudonym));
  if (version == 1) {
    CurvePoint localPseudonymAsCurvePoint(localPseudonymString);
    localPseudonymString = Serialization::ToString<CurvePoint>(localPseudonymAsCurvePoint);
  }
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << localPseudonymString << '\0' << participantGroup << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

LocalPseudonym ParticipantGroupParticipantRecord::getLocalPseudonym() const {
  return LocalPseudonym::FromPacked(SpanToString(localPseudonym));
}

LocalPseudonym ParticipantGroupParticipant::getLocalPseudonym() const {
  return LocalPseudonym::FromPacked(SpanToString(localPseudonym));
}


ColumnGroupRecord::ColumnGroupRecord(std::string name, bool tombstone) : tombstone(tombstone), name(std::move(name)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ColumnGroupRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << name << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}


ColumnRecord::ColumnRecord(std::string name, bool tombstone) : tombstone(tombstone), name(std::move(name)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ColumnRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << name << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

ColumnGroupColumnRecord::ColumnGroupColumnRecord(
  std::string column,
  std::string columnGroup,
  bool tombstone) : tombstone(tombstone), columnGroup(std::move(columnGroup)), column(std::move(column)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ColumnGroupColumnRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << column << '\0' << columnGroup << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

ColumnGroupAccessRuleRecord::ColumnGroupAccessRuleRecord(
  std::string columnGroup,
  std::string userGroup,
  std::string mode,
  bool tombstone) : tombstone(tombstone), columnGroup(std::move(columnGroup)), userGroup(std::move(userGroup)), mode(std::move(mode)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ColumnGroupAccessRuleRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << userGroup << '\0' << mode << '\0'
    << columnGroup << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

ParticipantGroupAccessRuleRecord::ParticipantGroupAccessRuleRecord(
  std::string group,
  std::string userGroup,
  std::string mode,
  bool tombstone) : tombstone(tombstone), participantGroup(std::move(group)), userGroup(std::move(userGroup)), mode(std::move(mode)) {
  RandomBytes(checksumNonce, 16);
  this->timestamp = Timestamp().getTime();
}

uint64_t ParticipantGroupAccessRuleRecord::checksum() const {
  std::ostringstream os;
  os << std::string(checksumNonce.begin(), checksumNonce.end())
    << timestamp << '\0' << participantGroup << '\0' << mode << '\0'
    << userGroup << '\0' << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

ColumnNameMapping ColumnNameMappingRecord::toLiveObject() const {
  ColumnNameMapping result{ColumnNameSection(original), ColumnNameSection(mapped)};
  assert(result.mapped.getValue() == mapped);
  return result;
}

ColumnNameMappingRecord ColumnNameMappingRecord::FromLiveObject(const ColumnNameMapping& mapping) {
  return ColumnNameMappingRecord{mapping.original.getValue(), mapping.mapped.getValue()};
}

StructureMetadataRecord::StructureMetadataRecord(
    StructureMetadataType subjectType,
    std::string subject,
    std::string metadataGroup,
    std::string key,
    std::vector<char> value,
    bool tombstone)
  : timestamp{Timestamp().getTime()},
    tombstone{tombstone},
    subjectType{ToUnderlying(subjectType)},
    subject(std::move(subject)),
    metadataGroup(std::move(metadataGroup)),
    subkey(std::move(key)),
    value(std::move(value)) {
  assert((!this->tombstone || this->value.empty()) && "Tombstone with non-empty value");
  RandomBytes(checksumNonce, 16);
}

uint64_t StructureMetadataRecord::checksum() const {
  std::ostringstream os;
  os << RangeToCollection<std::string_view>(checksumNonce)
    << timestamp << '\0' << subjectType << '\0' << subject << '\0' << metadataGroup << '\0' << subkey << '\0'
    << RangeToCollection<std::string_view>(value) << '\0'
    << tombstone;
  return UnpackUint64BE(Sha256().digest(std::move(os).str()));
}

} // namespace pep
