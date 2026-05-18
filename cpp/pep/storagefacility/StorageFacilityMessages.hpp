#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/serialization/IndexList.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

class DataEnumerationRequest2 {
public:
  SignedTicket2 mTicket;
  // Falls back to all columns in ticket
  std::optional<IndexList> mColumns{};
  // Falls back to all pseudonyms in ticket
  std::optional<IndexList> mPseudonyms{};
};

class DataEnumerationEntry2 {
public:
  std::string mId;
  Metadata mMetadata;
  EncryptedKey mPolymorphicKey;
  uint64_t mFileSize{};
  uint32_t mColumnIndex{};
  uint32_t mPseudonymIndex{};
  uint32_t mIndex = 0;
};

class DataEnumerationResponse2 {
public:
  std::vector<DataEnumerationEntry2> mEntries;
  bool mHasMore = false;
};

class MetadataReadRequest2 {
public:
  SignedTicket2 mTicket;
  std::vector<std::string> mIds;
};

class DataReadRequest2 {
public:
  SignedTicket2 mTicket;
  std::vector<std::string> mIds;
};

class DataRequestEntry2 {
public:
  uint32_t mColumnIndex{};
  uint32_t mPseudonymIndex{};
};

class DataStoreEntry2 : public DataRequestEntry2 {
public:
  Metadata mMetadata;
  EncryptedKey mPolymorphicKey;
};

// Base class for request classes that specify the entries they manipulate.
// Derive (i.e. don't typedef) implementors so that message types can be identified by their MessageMagic.
template <typename TEntry>
class DataEntriesRequest2 {
public:
  using Entry = TEntry;

  SignedTicket2 mTicket;
  std::vector<Entry> mEntries;
};

class MetadataUpdateRequest2 : public DataEntriesRequest2<DataStoreEntry2> {
};

class MetadataUpdateResponse2 {
public:
  std::vector<std::string> mIds;
};

class DataStoreRequest2 : public DataEntriesRequest2<DataStoreEntry2> {
};

class DataStoreResponse2 : public MetadataUpdateResponse2 {
public:
  uint64_t mHash{};
};

class DataDeleteRequest2 : public DataEntriesRequest2<DataRequestEntry2> {
};

class DataDeleteResponse2 {
public:
  Timestamp mTimestamp;
  IndexList mEntries; // Indices correspond with DataDeleteRequest2's mEntries
};

class DataHistoryRequest2 {
public:
  SignedTicket2 mTicket;
  std::optional<IndexList> mColumns;
  std::optional<IndexList> mPseudonyms;
};

class DataHistoryEntry2 {
public:
  uint32_t mColumnIndex{}; // In the DataHistoryRequest2::mTicket
  uint32_t mPseudonymIndex{}; // In the DataHistoryRequest2::mTicket
  Timestamp mTimestamp;
  std::string mId; // Storage facility ID. If empty, this history entry represents a deletion
};

class DataHistoryResponse2 {
public:
  std::vector<DataHistoryEntry2> mEntries;
};

using SignedDataEnumerationRequest2 = Signed<DataEnumerationRequest2>;
using SignedMetadataReadRequest2 = Signed<MetadataReadRequest2>;
using SignedDataReadRequest2 = Signed<DataReadRequest2>;
using SignedMetadataUpdateRequest2 = Signed<MetadataUpdateRequest2>;
using SignedDataStoreRequest2 = Signed<DataStoreRequest2>;
using SignedDataDeleteRequest2 = Signed<DataDeleteRequest2>;
using SignedDataHistoryRequest2 = Signed<DataHistoryRequest2>;

}
