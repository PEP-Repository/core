#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

struct DataEnumerationRequest2 {
  SignedTicket2 ticket;
  // Falls back to all columns in ticket
  std::optional<IndexList> columns{};
  // Falls back to all pseudonyms in ticket
  std::optional<IndexList> pseudonyms{};
};

struct DataEnumerationEntry2 {
  std::string id;
  Metadata metadata;
  EncryptedKey polymorphicKey;
  uint64_t fileSize{};
  uint32_t columnIndex{};
  uint32_t pseudonymIndex{};
  uint32_t index = 0;
};

struct DataEnumerationResponse2 {
  std::vector<DataEnumerationEntry2> entries;
  bool hasMore = false;
};

struct MetadataReadRequest2 {
  SignedTicket2 ticket;
  std::vector<std::string> ids;
};

struct DataReadRequest2 {
  SignedTicket2 ticket;
  std::vector<std::string> ids;
};

struct DataRequestEntry2 {
  uint32_t columnIndex{};
  uint32_t pseudonymIndex{};
};

struct DataStoreEntry2 : public DataRequestEntry2 {
  Metadata metadata;
  EncryptedKey polymorphicKey;
};

// Base class for request classes that specify the entries they manipulate.
// Derive (i.e. don't typedef) implementors so that message types can be identified by their MessageMagic.
template <typename TEntry>
struct DataEntriesRequest2 {
  using Entry = TEntry;

  SignedTicket2 ticket;
  std::vector<Entry> entries;
};

struct MetadataUpdateRequest2 : public DataEntriesRequest2<DataStoreEntry2> {};

struct MetadataUpdateResponse2 {
  std::vector<std::string> ids;
};

struct DataStoreRequest2 : public DataEntriesRequest2<DataStoreEntry2> {};

struct DataStoreResponse2 : public MetadataUpdateResponse2 {
  uint64_t hash{};
};

struct DataDeleteRequest2 : public DataEntriesRequest2<DataRequestEntry2> {};

struct DataDeleteResponse2 {
  Timestamp timestamp;
  IndexList entries; // Indices correspond with DataDeleteRequest2's entries_
};

struct DataHistoryRequest2 {
  SignedTicket2 ticket;
  std::optional<IndexList> columns;
  std::optional<IndexList> pseudonyms;
};

struct DataHistoryEntry2 {
  uint32_t columnIndex{}; // In the DataHistoryRequest2::ticket_
  uint32_t pseudonymIndex{}; // In the DataHistoryRequest2::ticket_
  Timestamp timestamp;
  std::string id; // Storage facility ID. If empty, this history entry represents a deletion
};

struct DataHistoryResponse2 {
  std::vector<DataHistoryEntry2> entries;
};

struct DataSizeRequest {
  std::set<std::string> columns;
};

struct DataSizeResponse {
  uint64_t blockSize;
  uint64_t totalBlocks;
  uint64_t rollingBlocks;
};

using SignedDataEnumerationRequest2 = Signed<DataEnumerationRequest2>;
using SignedMetadataReadRequest2 = Signed<MetadataReadRequest2>;
using SignedDataReadRequest2 = Signed<DataReadRequest2>;
using SignedMetadataUpdateRequest2 = Signed<MetadataUpdateRequest2>;
using SignedDataStoreRequest2 = Signed<DataStoreRequest2>;
using SignedDataDeleteRequest2 = Signed<DataDeleteRequest2>;
using SignedDataHistoryRequest2 = Signed<DataHistoryRequest2>;
using SignedDataSizeRequest = Signed<DataSizeRequest>;

}
