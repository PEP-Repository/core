#pragma once

#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/crypto/Signed.hpp>
#include <pep/structure/ColumnName.hpp>
#include <pep/auth/UserGroup.hpp>

#include <compare>
#include <cstdint>
#include <ostream>

namespace pep {

class GlobalConfigurationRequest {
public:
  GlobalConfigurationRequest() = default;
};

/// A \c SignedTicket2 with added mappings from column groups and participant groups to
/// respectively their columns and participants
class IndexedTicket2 {
protected:
  // Cache unpacked version of ticket.
  // TODO can we do this without a mutex?
  mutable std::shared_ptr<Ticket2> mUnpackedTicket;
  mutable std::mutex mUnpackedTicketLock;

  std::shared_ptr<SignedTicket2> mTicket;
  /// Maps column group names to indices of their column names in \c Ticket2.mColumns
  std::unordered_map<std::string, IndexList> mColumnGroups;
  /// Maps participant group names to indices of their pseudonyms in \c Ticket2.mPseudonyms
  std::unordered_map<std::string, IndexList> mParticipantGroups;

public:
  IndexedTicket2(const IndexedTicket2& other) :
    mTicket(other.mTicket),
    mColumnGroups(other.mColumnGroups),
    mParticipantGroups(other.mParticipantGroups) {
    std::lock_guard<std::mutex> lock(other.mUnpackedTicketLock);
    mUnpackedTicket = other.mUnpackedTicket;
  }
  IndexedTicket2(IndexedTicket2&& other) :
    mTicket(std::move(other.mTicket)),
    mColumnGroups(std::move(other.mColumnGroups)),
    mParticipantGroups(std::move(other.mParticipantGroups)) {
    std::lock_guard<std::mutex> lock(other.mUnpackedTicketLock);
    mUnpackedTicket = other.mUnpackedTicket;
  }
  IndexedTicket2(
    std::shared_ptr<SignedTicket2> ticket,
    std::unordered_map<std::string, IndexList> columnGroups,
    std::unordered_map<std::string, IndexList> participantGroups)
    : mTicket(std::move(ticket)), mColumnGroups(std::move(columnGroups)),
    mParticipantGroups(std::move(participantGroups)) { }

  std::shared_ptr<Ticket2> openTicketWithoutCheckingSignature() const;
  std::shared_ptr<SignedTicket2> getTicket() const;
  const std::unordered_map<std::string, IndexList>& getColumnGroupMapping() const;
  const std::unordered_map<std::string, IndexList>& getParticipantGroupMapping() const;
  std::vector<std::string> getColumnGroups() const;
  std::vector<std::string> getParticipantGroups() const;
  std::vector<std::string> getColumns() const;
  std::vector<std::string> getModes() const;
  std::vector<PolymorphicPseudonym> getPolymorphicPseudonyms() const;
};

enum KeyBlindMode {
  BLIND_MODE_UNKNOWN = 0,
  BLIND_MODE_BLIND = 1,
  BLIND_MODE_UNBLIND = 2
};

class KeyRequestEntry {
public:
  KeyRequestEntry() = default;
  inline KeyRequestEntry(
    Metadata metadata,
    EncryptedKey polymorphEncryptionKey,
    KeyBlindMode keyBlindMode,
    uint32_t pseudonymIndex = 0
  ) : mMetadata(std::move(metadata)),
    mPolymorphEncryptionKey(polymorphEncryptionKey),
    mKeyBlindMode(keyBlindMode),
    mPseudonymIndex(pseudonymIndex) {}

  Metadata mMetadata;
  EncryptedKey mPolymorphEncryptionKey;
  KeyBlindMode mKeyBlindMode = KeyBlindMode::BLIND_MODE_UNKNOWN;
  uint32_t mPseudonymIndex{};
};

class EncryptionKeyRequest {
public:
  std::shared_ptr<SignedTicket2> mTicket2;
  std::vector<KeyRequestEntry> mEntries;
};

class EncryptionKeyResponse {
public:
  std::vector<EncryptedKey> mKeys;
};

class ColumnAccess {
public:
  class GroupProperties {
  public:
    std::vector<std::string> modes;
    IndexList columns;
    std::strong_ordering operator<=>(const GroupProperties& other) const = default;
  };
  std::unordered_map<std::string, GroupProperties> columnGroups;
  std::vector<std::string> columns;
};

class ColumnAccessRequest {
public:
  bool includeImplicitlyGranted = false;
  std::vector<std::string> requireModes;
};

using ColumnAccessResponse = ColumnAccess;

class ParticipantGroupAccess {
public:
  using modes = std::vector<std::string>;
  std::unordered_map<std::string, modes> participantGroups;
};

class ParticipantGroupAccessRequest {
public:
  bool includeImplicitlyGranted = false;
};

using ParticipantGroupAccessResponse = ParticipantGroupAccess;

enum CrudAction { Create, Read, Update, Delete };

struct ColumnNameMappingRequest {
  CrudAction action = CrudAction::Read;
  std::optional<ColumnNameSection> original;
  std::optional<ColumnNameSection> mapped;
};

struct ColumnNameMappingResponse {
  std::vector<ColumnNameMapping> mappings;
};

struct MigrateUserDbToAccessManagerRequest {};
struct MigrateUserDbToAccessManagerResponse {};

class FindUserRequest {
public:
  FindUserRequest() = default;
  FindUserRequest(std::string primaryId, std::vector<std::string> alternativeIds) : mPrimaryId(std::move(primaryId)), mAlternativeIds(std::move(alternativeIds)) { }

  std::string mPrimaryId;
  std::vector<std::string> mAlternativeIds;
};

class FindUserResponse {
public:
  FindUserResponse() = default;
  FindUserResponse(std::optional<std::vector<UserGroup>> userGroups) : mUserGroups(std::move(userGroups)) { }

  /// nullopt if the user doesn't exist. Otherwise the list of user groups the user is in.
  std::optional<std::vector<UserGroup>> mUserGroups;
};

struct StructureMetadataKey {
  std::string metadataGroup;
  std::string subkey;

  auto operator<=>(const StructureMetadataKey&) const = default;

  /// Get as metadataGroup:subkey
  [[nodiscard]] std::string toString() const;

  friend std::ostream& operator<<(std::ostream& out, const StructureMetadataKey& key) {
    return out << key.toString();
  }
};

struct StructureMetadataSubjectKey {
  std::string subject;
  StructureMetadataKey key;

  auto operator<=>(const StructureMetadataSubjectKey&) const = default;
};

/// Structure (non-cell) metadata entry
struct StructureMetadataEntry {
  StructureMetadataSubjectKey subjectKey;
  std::string value;

  auto operator<=>(const StructureMetadataEntry&) const = default;
};

enum class StructureMetadataType : uint8_t {
  // Also add new members to Messages.proto
  Column,
  ColumnGroup,
  ParticipantGroup,
  User,
  UserGroup,
};

/// \see StructureMetadataFilter
struct StructureMetadataRequest {
  StructureMetadataType subjectType{};
  std::vector<std::string> subjects;
  std::vector<StructureMetadataKey> keys;
};

struct SetStructureMetadataRequest {
  StructureMetadataType subjectType{};
  std::vector<StructureMetadataSubjectKey> remove{};
};

struct SetStructureMetadataResponse {};


using SignedEncryptionKeyRequest = Signed<EncryptionKeyRequest>;
using SignedColumnAccessRequest = Signed<ColumnAccessRequest>;
using SignedParticipantGroupAccessRequest = Signed<ParticipantGroupAccessRequest>;
using SignedColumnNameMappingRequest = Signed<ColumnNameMappingRequest>;
using SignedMigrateUserDbToAccessManagerRequest = Signed<MigrateUserDbToAccessManagerRequest>;
using SignedFindUserRequest = Signed<FindUserRequest>;
using SignedStructureMetadataRequest = Signed<StructureMetadataRequest>;
using SignedSetStructureMetadataRequest = Signed<SetStructureMetadataRequest>;

}
