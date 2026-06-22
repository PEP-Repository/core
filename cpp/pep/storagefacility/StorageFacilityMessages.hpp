#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

class DataEnumerationRequest2 {
public:
  SignedTicket2 ticket_;
  // Falls back to all columns in ticket
  std::optional<IndexList> columns_{};
  // Falls back to all pseudonyms in ticket
  std::optional<IndexList> pseudonyms_{};
};

class DataEnumerationEntry2 {
public:
  std::string id_;
  Metadata metadata_;
  EncryptedKey polymorphicKey_;
  uint64_t fileSize_{};
  uint32_t columnIndex_{};
  uint32_t pseudonymIndex_{};
  uint32_t index_ = 0;
};

class DataEnumerationResponse2 {
public:
  std::vector<DataEnumerationEntry2> entries_;
  bool hasMore_ = false;
};

class MetadataReadRequest2 {
public:
  SignedTicket2 ticket_;
  std::vector<std::string> ids_;
};

class DataReadRequest2 {
public:
  SignedTicket2 ticket_;
  std::vector<std::string> ids_;
};

class DataRequestEntry2 {
public:
  uint32_t columnIndex_{};
  uint32_t pseudonymIndex_{};
};

class DataStoreEntry2 : public DataRequestEntry2 {
public:
  Metadata metadata_;
  EncryptedKey polymorphicKey_;
};

// Base class for request classes that specify the entries they manipulate.
// Derive (i.e. don't typedef) implementors so that message types can be identified by their MessageMagic.
template <typename TEntry>
class DataEntriesRequest2 {
public:
  using Entry = TEntry;

  SignedTicket2 ticket_;
  std::vector<Entry> entries_;
};

class MetadataUpdateRequest2 : public DataEntriesRequest2<DataStoreEntry2> {
};

class MetadataUpdateResponse2 {
public:
  std::vector<std::string> ids_;
};

class DataStoreRequest2 : public DataEntriesRequest2<DataStoreEntry2> {
};

class DataStoreResponse2 : public MetadataUpdateResponse2 {
public:
  uint64_t hash_{};
};

class DataDeleteRequest2 : public DataEntriesRequest2<DataRequestEntry2> {
};

class DataDeleteResponse2 {
public:
  Timestamp timestamp_;
  IndexList entries_; // Indices correspond with DataDeleteRequest2's entries_
};

class DataHistoryRequest2 {
public:
  SignedTicket2 ticket_;
  std::optional<IndexList> columns_;
  std::optional<IndexList> pseudonyms_;
};

class DataHistoryEntry2 {
public:
  uint32_t columnIndex_{}; // In the DataHistoryRequest2::ticket_
  uint32_t pseudonymIndex_{}; // In the DataHistoryRequest2::ticket_
  Timestamp timestamp_;
  std::string id_; // Storage facility ID. If empty, this history entry represents a deletion
};

class DataHistoryResponse2 {
public:
  std::vector<DataHistoryEntry2> entries_;
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
