#pragma once

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/ColumnName.hpp>

namespace pep {
// All record types also have an acssociated struct type containing only the data about the columns, access modes, etc. So no tombstones, timestamps, seqno's, and other metadata.
// This allows for sets of named structs that can be equality checked on only these datapoints.

inline bool HasInternalId(StructureMetadataType subjectType) {
  return subjectType == StructureMetadataType::User || subjectType == StructureMetadataType::UserGroup;
}

struct StructureMetadataFilter {
  /// Names of subjects to include (e.g. column names).
  /// Leave empty to include all subjects.
  std::vector<std::string> subjects{};
  /// Metadata keys to include.
  /// Specifying just \a metadataGroup and leaving \a key blank acts like a wildcard.
  std::vector<StructureMetadataKey> keys{};
};


struct SelectStarPseudonymRecord {
  SelectStarPseudonymRecord() = default;
  SelectStarPseudonymRecord(LocalPseudonym lp, PolymorphicPseudonym pp);
  uint64_t checksum(int version) const;

  LocalPseudonym getLocalPseudonym() const;
  PolymorphicPseudonym getPolymorphicPseudonym() const;

  int64_t seqno{};
  std::vector<char> localPseudonym;
  std::vector<char> polymorphicPseudonym;
};


/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should be in the vector "columns".
*/
struct ColumnFilter {
  std::optional<std::vector<std::string>> columns = std::nullopt;
};

struct ColumnRecord {
  ColumnRecord() = default;
  ColumnRecord(std::string name, bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string name;
  static inline const std::tuple RecordIdentifier{&ColumnRecord::name};
};

struct Column {
  std::string name;

  Column(std::string name) : name(name) {}
  std::strong_ordering operator<=>(const Column& other) const = default;
};

struct ColumnNameMappingRecord {
  std::string original;
  std::string mapped;

  ColumnNameMapping toLiveObject() const;
  static ColumnNameMappingRecord FromLiveObject(const ColumnNameMapping& mapping);
};

/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should be in the vector "columnGroups".
*/
struct ColumnGroupFilter {
  std::optional<std::vector<std::string>> columnGroups = std::nullopt;
};

struct ColumnGroupRecord {
  ColumnGroupRecord() = default;
  ColumnGroupRecord(std::string name, bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string name;
  static inline const std::tuple RecordIdentifier{&ColumnGroupRecord::name};
};

struct ColumnGroup {
  std::string name;

  ColumnGroup(std::string name) : name(name) {}
  std::strong_ordering operator<=>(const ColumnGroup& other) const = default;
};

/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should have a columnGroup specified in ColumnGroups AND a column specified in columns.
*/
struct ColumnGroupColumnFilter {
  std::optional<std::vector<std::string>> columnGroups = std::nullopt;
  std::optional<std::vector<std::string>> columns = std::nullopt;
};

struct ColumnGroupColumnRecord {
  ColumnGroupColumnRecord() = default;
  ColumnGroupColumnRecord(
    std::string column,
    std::string columnGroup,
    bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string columnGroup;
  std::string column;
  static inline const std::tuple RecordIdentifier{
    &ColumnGroupColumnRecord::columnGroup,
    &ColumnGroupColumnRecord::column,
  };
};

struct ColumnGroupColumn {
  std::string columnGroup;
  std::string column;

  ColumnGroupColumn(std::string columnGroup,std::string column) : columnGroup(columnGroup), column(column) {}
  std::strong_ordering operator<=>(const ColumnGroupColumn& other) const = default;
};

/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should have a columnGroup specified in coluimnGroups AND a userGroup specified in userGroups AND a mode specified in modes.
*/
struct ColumnGroupAccessRuleFilter {
  std::optional<std::vector<std::string>> columnGroups = std::nullopt;
  std::optional<std::vector<std::string>> userGroups = std::nullopt;
  std::optional<std::vector<std::string>> modes = std::nullopt;
};

struct ColumnGroupAccessRuleRecord {
  ColumnGroupAccessRuleRecord() = default;
  ColumnGroupAccessRuleRecord(
    std::string columnGroup,
    std::string userGroup,
    std::string mode,
    bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string columnGroup;
  std::string userGroup;
  std::string mode;
  static inline const std::tuple RecordIdentifier{
    &ColumnGroupAccessRuleRecord::columnGroup,
    &ColumnGroupAccessRuleRecord::userGroup,
    &ColumnGroupAccessRuleRecord::mode,
  };
};

struct ColumnGroupAccessRule {
  std::string columnGroup;
  std::string userGroup;
  std::string mode;

  ColumnGroupAccessRule(std::string columnGroup, std::string userGroup, std::string mode) : columnGroup(columnGroup), userGroup(userGroup), mode(mode) {}

  std::strong_ordering operator<=>(const ColumnGroupAccessRule& other) const = default;
};
/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should have a participantGroup specified in participantGroups.
*/
struct ParticipantGroupFilter {
  std::optional<std::vector<std::string>> participantGroups = std::nullopt;
};

struct ParticipantGroupRecord {
  ParticipantGroupRecord() = default;
  ParticipantGroupRecord(std::string name, bool tombstone = false);
  uint64_t checksum() const;


  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string name;
  static inline const std::tuple RecordIdentifier{&ParticipantGroupRecord::name};
};

struct ParticipantGroup {
  std::string name;

  ParticipantGroup(std::string name) : name(name) {}
  std::strong_ordering operator<=>(const ParticipantGroup& other) const = default;
};

/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should have a participantGroup specified in participantGroups AND a localPseudonym specified in localPseudonyms.
*/
struct ParticipantGroupParticipantFilter {
  std::optional<std::vector<std::string>> participantGroups = std::nullopt;
  std::optional<std::vector<LocalPseudonym>> localPseudonyms = std::nullopt;
};

struct ParticipantGroupParticipantRecord {
  ParticipantGroupParticipantRecord() = default;
  ParticipantGroupParticipantRecord(
      LocalPseudonym localPseudonym,
    std::string participantGroup,
    bool tombstone = false);
  uint64_t checksum(int version) const;

  LocalPseudonym getLocalPseudonym() const;


  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string participantGroup;
  std::vector<char> localPseudonym;
  static inline const std::tuple RecordIdentifier{
    &ParticipantGroupParticipantRecord::participantGroup,
    &ParticipantGroupParticipantRecord::localPseudonym,
  };
};

struct ParticipantGroupParticipant {
  std::string participantGroup;
  std::vector<char> localPseudonym;

  ParticipantGroupParticipant(std::string participantGroup, std::vector<char> localPseudonym) : participantGroup(participantGroup), localPseudonym(localPseudonym) {}
  LocalPseudonym getLocalPseudonym() const;

  std::strong_ordering operator<=>(const ParticipantGroupParticipant& other) const = default;
};

/* Contains vectors of strings used by AccessManager::Backend::Storage to filter results.
 * Returned results should have a participantGroup specified in participantGroups AND a userGroup specified in userGroups AND a mode specified in modes.
*/
struct ParticipantGroupAccessRuleFilter {
  std::optional<std::vector<std::string>> participantGroups = std::nullopt;
  std::optional<std::vector<std::string>> userGroups = std::nullopt;
  std::optional<std::vector<std::string>> modes = std::nullopt;
};

struct ParticipantGroupAccessRuleRecord {
  ParticipantGroupAccessRuleRecord() = default;
  ParticipantGroupAccessRuleRecord(
    std::string group,
    std::string userGroup,
    std::string mode,
    bool tombstone = false);
  uint64_t checksum() const;


  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::string participantGroup;
  std::string userGroup;
  std::string mode;
  static inline const std::tuple RecordIdentifier{
    &ParticipantGroupAccessRuleRecord::participantGroup,
    &ParticipantGroupAccessRuleRecord::userGroup,
    &ParticipantGroupAccessRuleRecord::mode,
  };
};

struct ParticipantGroupAccessRule {
  std::string participantGroup;
  std::string userGroup;
  std::string mode;

  ParticipantGroupAccessRule(std::string participantGroup, std::string userGroup, std::string mode) : participantGroup(participantGroup), userGroup(userGroup), mode(mode) {}

  std::strong_ordering operator<=>(const ParticipantGroupAccessRule& other) const = default;
};

struct StructureMetadataRecord {
  StructureMetadataRecord() = default;
  StructureMetadataRecord(
    StructureMetadataType subjectType,
    std::string subject,
    std::string metadataGroup,
    std::string key,
    std::vector<char> value,
    bool tombstone = false);
  // I would have liked to include the `subject` field as well for records with an internalSubjectId, so we can store the
  // actual subject name that was used when creating the record. This could then be used for e.g.
  // Storage::getSomeSubjectForInternalId, to know which subject name to choose.
  // But this does not work nicely with e.g. GetCurrentRecords, unless you make sure you always store the original subject
  // name for follow-up records of the same metadata entry.
  // For example, let's say a user does the following:
  //   pepcli user create JohnSmith
  //   pepcli structure-metadata user set --key foo:bar --value hello JohnSmith
  //   pepcli user addIdentifier JohnS
  //   pepcli user removeIdentifier JohnSmith
  //   pepcli structure-metadata user set --key foo:bar --value helloAgain JohnS
  // You might expect the record that is created in the last step to use JohnS as subject name. But you need to use JohnSmith,
  // even though that identifier no longer exists.
   StructureMetadataRecord(
     StructureMetadataType subjectType,
     int64_t internalSubjectId,
     std::string metadataGroup,
     std::string key,
     std::vector<char> value,
     bool tombstone = false);
  uint64_t checksum() const;

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t timestamp{};
  bool tombstone{};

  std::underlying_type_t<StructureMetadataType> subjectType{};
  std::string subject;
  std::optional<int64_t> internalSubjectId;
  std::string metadataGroup;
  std::string subkey;
  static inline const std::tuple RecordIdentifier{
    &StructureMetadataRecord::subjectType,
    &StructureMetadataRecord::subject,
    &StructureMetadataRecord::internalSubjectId,
    &StructureMetadataRecord::metadataGroup,
    &StructureMetadataRecord::subkey,
  };

  std::vector<char> value;

  StructureMetadataType getSubjectType() const {
    return static_cast<StructureMetadataType>(subjectType);
  }
};

} // namespace pep
