#pragma once

#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/morphing/ServerVerifiers.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/auth/Signed.hpp>
#include <pep/structure/ColumnName.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/serialization/Serializer.hpp>

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
  mutable std::shared_ptr<Ticket2> unpackedTicket_;
  mutable std::mutex unpackedTicketLock_;

  std::shared_ptr<SignedTicket2> ticket_;
  /// Maps column group names to indices of their column names in \c Ticket2.columns_
  std::unordered_map<std::string, IndexList> columnGroups_;
  /// Maps participant group names to indices of their pseudonyms in \c Ticket2.pseudonyms_
  std::unordered_map<std::string, IndexList> participantGroups_;

public:
  IndexedTicket2(const IndexedTicket2& other) :
    ticket_(other.ticket_),
    columnGroups_(other.columnGroups_),
    participantGroups_(other.participantGroups_) {
    std::lock_guard<std::mutex> lock(other.unpackedTicketLock_);
    unpackedTicket_ = other.unpackedTicket_;
  }
  IndexedTicket2(IndexedTicket2&& other) :
    ticket_(std::move(other.ticket_)),
    columnGroups_(std::move(other.columnGroups_)),
    participantGroups_(std::move(other.participantGroups_)) {
    std::lock_guard<std::mutex> lock(other.unpackedTicketLock_);
    unpackedTicket_ = other.unpackedTicket_;
  }
  IndexedTicket2(
    std::shared_ptr<SignedTicket2> ticket,
    std::unordered_map<std::string, IndexList> columnGroups,
    std::unordered_map<std::string, IndexList> participantGroups)
    : ticket_(std::move(ticket)), columnGroups_(std::move(columnGroups)),
    participantGroups_(std::move(participantGroups)) { }

  std::shared_ptr<Ticket2> openTicketWithoutCheckingSignature() const;
  std::shared_ptr<SignedTicket2> getTicket() const;
  const std::unordered_map<std::string, IndexList>& getColumnGroupMapping() const;
  const std::unordered_map<std::string, IndexList>& getParticipantGroupMapping() const;
  std::vector<std::string> getColumnGroups() const;
  std::vector<std::string> getParticipantGroups() const;
  std::vector<std::string> getColumns() const;
  std::vector<std::string> getModes() const;
  std::vector<PolymorphicPseudonym> getAccessSubjects() const;
};

enum class KeyBlindMode {
  Unknown = 0,
  Blind = 1,
  Unblind = 2
};

class KeyRequestEntry {
public:
  KeyRequestEntry() = default;
  inline KeyRequestEntry(
    const Metadata& metadata,
    EncryptedKey polymorphEncryptionKey,
    KeyBlindMode keyBlindMode,
    uint32_t pseudonymIndex = 0
  ) : metadata_(metadata.getBound()),
    polymorphEncryptionKey_(polymorphEncryptionKey),
    keyBlindMode_(keyBlindMode),
    pseudonymIndex_(pseudonymIndex) {}

  Metadata metadata_;
  EncryptedKey polymorphEncryptionKey_;
  KeyBlindMode keyBlindMode_ = KeyBlindMode::Unknown;
  uint32_t pseudonymIndex_{};
};

class EncryptionKeyRequest {
public:
  std::shared_ptr<SignedTicket2> ticket2_;
  std::vector<KeyRequestEntry> entries_;
};

class EncryptionKeyResponse {
public:
  std::vector<EncryptedKey> keys_;
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
  FindUserRequest(std::string primaryId, std::vector<std::string> alternativeIds) : primaryId_(std::move(primaryId)), alternativeIds_(std::move(alternativeIds)) { }

  std::string primaryId_;
  std::vector<std::string> alternativeIds_;
};

class FindUserResponse {
public:
  FindUserResponse() = default;
  FindUserResponse(std::optional<std::vector<UserGroup>> userGroups) : userGroups_(std::move(userGroups)) { }

  /// nullopt if the user doesn't exist. Otherwise the list of user groups the user is in.
  std::optional<std::vector<UserGroup>> userGroups_;
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

class VerifiersRequest {
};

class VerifiersResponse {
  friend Serializer<VerifiersResponse>;

  ServerVerifiers verifiers_;
  ReshuffleRekeyVerifiersProof
    accessManagerProof_,
    storageFacilityProof_,
    transcryptorProof_;

public:
  VerifiersResponse(
    const ServerVerifiers& verifiers,
    const ReshuffleRekeyVerifiersProof& accessManagerProof,
    const ReshuffleRekeyVerifiersProof& storageFacilityProof,
    const ReshuffleRekeyVerifiersProof& transcryptorProof)
    : verifiers_(verifiers),
      accessManagerProof_(accessManagerProof),
      storageFacilityProof_(storageFacilityProof),
      transcryptorProof_(transcryptorProof) {}

  VerifiersResponse(
    const ReshuffleRekeyVerifiersWithProof& accessManager,
    const ReshuffleRekeyVerifiersWithProof& storageFacility,
    const ReshuffleRekeyVerifiersWithProof& transcryptor)
    : verifiers_{
        .accessManager = accessManager.first,
        .storageFacility = storageFacility.first,
        .transcryptor = transcryptor.first,
      },
      accessManagerProof_(accessManager.second),
      storageFacilityProof_(storageFacility.second),
      transcryptorProof_(transcryptor.second) {}

  ServerVerifiers open(const ElgamalPublicKey& globalKey) const {
    accessManagerProof_.verify(verifiers_.accessManager, globalKey);
    storageFacilityProof_.verify(verifiers_.storageFacility, globalKey);
    transcryptorProof_.verify(verifiers_.transcryptor, globalKey);
    return verifiers_;
  }
};

struct UserVerifiersRequest {
  X509Certificate userCertificate;
};

class UserVerifiersResponse {
  friend Serializer<UserVerifiersResponse>;

  ReshuffleRekeyVerifiers verifiers_;
  ReshuffleRekeyVerifiersProof proof_;

public:
  UserVerifiersResponse(const ReshuffleRekeyVerifiersWithProof& verifiers)
    : verifiers_(verifiers.first), proof_(verifiers.second) {}

  ReshuffleRekeyVerifiers open(const ElgamalPublicKey& globalKey) const {
    proof_.verify(verifiers_, globalKey);
    return verifiers_;
  }
};


using SignedEncryptionKeyRequest = Signed<EncryptionKeyRequest>;
using SignedColumnAccessRequest = Signed<ColumnAccessRequest>;
using SignedParticipantGroupAccessRequest = Signed<ParticipantGroupAccessRequest>;
using SignedColumnNameMappingRequest = Signed<ColumnNameMappingRequest>;
using SignedMigrateUserDbToAccessManagerRequest = Signed<MigrateUserDbToAccessManagerRequest>;
using SignedFindUserRequest = Signed<FindUserRequest>;
using SignedStructureMetadataRequest = Signed<StructureMetadataRequest>;
using SignedSetStructureMetadataRequest = Signed<SetStructureMetadataRequest>;

}
