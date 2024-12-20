#pragma once

#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/crypto/Signed.hpp>
#include <pep/structure/ColumnName.hpp>

namespace pep {

class GlobalConfigurationRequest {
public:
  inline GlobalConfigurationRequest() {
  }
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
    : mTicket(ticket), mColumnGroups(std::move(columnGroups)),
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
  inline KeyRequestEntry() {}
  inline KeyRequestEntry(
    Metadata metadata,
    EncryptedKey polymorphEncryptionKey,
    KeyBlindMode keyBlindMode,
    uint32_t pseudonymIndex = 0
  ) : mMetadata(std::move(metadata)),
    mPolymorphEncryptionKey(std::move(polymorphEncryptionKey)),
    mKeyBlindMode(keyBlindMode),
    mPseudonymIndex(pseudonymIndex) {}

  Metadata mMetadata;
  EncryptedKey mPolymorphEncryptionKey;
  KeyBlindMode mKeyBlindMode = KeyBlindMode::BLIND_MODE_UNKNOWN;
  uint32_t mPseudonymIndex;
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


using SignedEncryptionKeyRequest = Signed<EncryptionKeyRequest>;
using SignedColumnAccessRequest = Signed<ColumnAccessRequest>;
using SignedParticipantGroupAccessRequest = Signed<ParticipantGroupAccessRequest>;
using SignedColumnNameMappingRequest = Signed<ColumnNameMappingRequest>;

}
