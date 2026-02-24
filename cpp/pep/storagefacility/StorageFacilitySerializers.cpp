#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/serialization/IndexListSerializer.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

namespace pep {

DataEnumerationRequest2 Serializer<DataEnumerationRequest2>::fromProtocolBuffer(proto::DataEnumerationRequest2&& source) const {
  DataEnumerationRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.mColumns = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.mPseudonyms = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataEnumerationRequest2>::moveIntoProtocolBuffer(proto::DataEnumerationRequest2& dest, DataEnumerationRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  if (value.mPseudonyms)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.mPseudonyms));
  if (value.mColumns)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.mColumns));
}

DataEnumerationEntry2 Serializer<DataEnumerationEntry2>::fromProtocolBuffer(proto::DataEnumerationEntry2&& source) const {
  DataEnumerationEntry2 result;
  result.mMetadata = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.mPolymorphicKey = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.mFileSize = source.file_size();
  result.mId = std::move(*source.mutable_id());
  result.mColumnIndex = source.column_index();
  result.mPseudonymIndex = source.pseudonym_index();
  result.mIndex = source.index();
  return result;
}

void Serializer<DataEnumerationEntry2>::moveIntoProtocolBuffer(proto::DataEnumerationEntry2& dest, DataEnumerationEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.mMetadata));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.mPolymorphicKey);
  dest.set_file_size(value.mFileSize);
  *dest.mutable_id() = std::move(value.mId);
  dest.set_column_index(value.mColumnIndex);
  dest.set_pseudonym_index(value.mPseudonymIndex);
  dest.set_index(value.mIndex);
}

DataEnumerationResponse2 Serializer<DataEnumerationResponse2>::fromProtocolBuffer(proto::DataEnumerationResponse2&& source) const {
  DataEnumerationResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  result.mHasMore = source.has_more();
  return result;
}

void Serializer<DataEnumerationResponse2>::moveIntoProtocolBuffer(proto::DataEnumerationResponse2& dest, DataEnumerationResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
  dest.set_has_more(value.mHasMore);
}

MetadataReadRequest2 Serializer<MetadataReadRequest2>::fromProtocolBuffer(proto::MetadataReadRequest2&& source) const {
  MetadataReadRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.mIds.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.mIds.push_back(std::move(x));
  return result;
}

void Serializer<MetadataReadRequest2>::moveIntoProtocolBuffer(proto::MetadataReadRequest2& dest, MetadataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  dest.mutable_ids()->Reserve(static_cast<int>(value.mIds.size()));
  for (auto x : value.mIds)
    dest.add_ids(std::move(x));
}

DataReadRequest2 Serializer<DataReadRequest2>::fromProtocolBuffer(proto::DataReadRequest2&& source) const {
  DataReadRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.mIds.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.mIds.push_back(std::move(x));
  return result;
}

void Serializer<DataReadRequest2>::moveIntoProtocolBuffer(proto::DataReadRequest2& dest, DataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  dest.mutable_ids()->Reserve(static_cast<int>(value.mIds.size()));
  for (auto x : value.mIds)
    dest.add_ids(std::move(x));
}

MetadataUpdateRequest2 Serializer<MetadataUpdateRequest2>::fromProtocolBuffer(proto::MetadataUpdateRequest2&& source) const {
  MetadataUpdateRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<MetadataUpdateRequest2>::moveIntoProtocolBuffer(proto::MetadataUpdateRequest2& dest, MetadataUpdateRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
}

MetadataUpdateResponse2 Serializer<MetadataUpdateResponse2>::fromProtocolBuffer(proto::MetadataUpdateResponse2&& source) const {
  MetadataUpdateResponse2 result;
  result.mIds.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.mIds.push_back(std::move(x));
  return result;
}

void Serializer<MetadataUpdateResponse2>::moveIntoProtocolBuffer(proto::MetadataUpdateResponse2& dest, MetadataUpdateResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.mIds.size()));
  for (auto x : value.mIds)
    dest.add_ids(std::move(x));
}

DataStoreResponse2 Serializer<DataStoreResponse2>::fromProtocolBuffer(proto::DataStoreResponse2&& source) const {
  DataStoreResponse2 result;
  result.mIds.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.mIds.push_back(std::move(x));
  result.mHash = source.hash();
  return result;
}

void Serializer<DataStoreResponse2>::moveIntoProtocolBuffer(proto::DataStoreResponse2& dest, DataStoreResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.mIds.size()));
  for (auto x : value.mIds)
    dest.add_ids(std::move(x));
  dest.set_hash(value.mHash);
}

DataStoreRequest2 Serializer<DataStoreRequest2>::fromProtocolBuffer(proto::DataStoreRequest2&& source) const {
  DataStoreRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataStoreRequest2>::moveIntoProtocolBuffer(proto::DataStoreRequest2& dest, DataStoreRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
}

DataStoreEntry2 Serializer<DataStoreEntry2>::fromProtocolBuffer(proto::DataStoreEntry2&& source) const {
  DataStoreEntry2 result;
  result.mMetadata = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.mPolymorphicKey = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.mColumnIndex = source.column_index();
  result.mPseudonymIndex = source.pseudonym_index();
  return result;
}

void Serializer<DataStoreEntry2>::moveIntoProtocolBuffer(proto::DataStoreEntry2& dest, DataStoreEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.mMetadata));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.mPolymorphicKey);
  dest.set_column_index(value.mColumnIndex);
  dest.set_pseudonym_index(value.mPseudonymIndex);
}

DataDeleteResponse2 Serializer<DataDeleteResponse2>::fromProtocolBuffer(proto::DataDeleteResponse2&& source) const {
  return {
    .mTimestamp = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .mEntries = Serialization::FromProtocolBuffer(std::move(*source.mutable_entries())),
  };
}

void Serializer<DataDeleteResponse2>::moveIntoProtocolBuffer(proto::DataDeleteResponse2& dest, DataDeleteResponse2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.mTimestamp);
}

DataDeleteRequest2 Serializer<DataDeleteRequest2>::fromProtocolBuffer(proto::DataDeleteRequest2&& source) const {
  DataDeleteRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataDeleteRequest2>::moveIntoProtocolBuffer(proto::DataDeleteRequest2& dest, DataDeleteRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
}

DataRequestEntry2 Serializer<DataRequestEntry2>::fromProtocolBuffer(proto::DataRequestEntry2&& source) const {
  DataRequestEntry2 result;
  result.mColumnIndex = source.column_index();
  result.mPseudonymIndex = source.pseudonym_index();
  return result;
}

void Serializer<DataRequestEntry2>::moveIntoProtocolBuffer(proto::DataRequestEntry2& dest, DataRequestEntry2 value) const {
  dest.set_column_index(value.mColumnIndex);
  dest.set_pseudonym_index(value.mPseudonymIndex);
}

DataHistoryRequest2 Serializer<DataHistoryRequest2>::fromProtocolBuffer(proto::DataHistoryRequest2&& source) const {
  DataHistoryRequest2 result;
  result.mTicket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.mColumns = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.mPseudonyms = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataHistoryRequest2>::moveIntoProtocolBuffer(proto::DataHistoryRequest2& dest, DataHistoryRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.mTicket));
  if (value.mPseudonyms)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.mPseudonyms));
  if (value.mColumns)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.mColumns));
}

DataHistoryEntry2 Serializer<DataHistoryEntry2>::fromProtocolBuffer(proto::DataHistoryEntry2&& source) const {
  return {
    .mColumnIndex = source.column_index(),
    .mPseudonymIndex = source.pseudonym_index(),
    .mTimestamp = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .mId = std::move(*source.mutable_id()),
  };
}

void Serializer<DataHistoryEntry2>::moveIntoProtocolBuffer(proto::DataHistoryEntry2& dest, DataHistoryEntry2 value) const {
  dest.set_column_index(value.mColumnIndex);
  dest.set_pseudonym_index(value.mPseudonymIndex);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.mTimestamp);
  *dest.mutable_id() = std::move(value.mId);
}

DataHistoryResponse2 Serializer<DataHistoryResponse2>::fromProtocolBuffer(proto::DataHistoryResponse2&& source) const {
  DataHistoryResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.mEntries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataHistoryResponse2>::moveIntoProtocolBuffer(proto::DataHistoryResponse2& dest, DataHistoryResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.mEntries));
}

DataPayloadPage Serializer<DataPayloadPage>::fromProtocolBuffer(proto::DataPayloadPage&& source) const {
  DataPayloadPage result;

  result.mCryptoNonce = std::move(*source.mutable_crypto_nonce());
  result.mCryptoMac = std::move(*source.mutable_crypto_mac());
  result.mPayloadData = std::move(*source.mutable_payload_data());
  result.mPageNumber = source.page_number();
  result.mIndex = source.index();

  return result;
}

void Serializer<DataPayloadPage>::moveIntoProtocolBuffer(proto::DataPayloadPage& dest, DataPayloadPage value) const {
  *dest.mutable_crypto_nonce() = std::move(value.mCryptoNonce);
  *dest.mutable_crypto_mac() = std::move(value.mCryptoMac);
  *dest.mutable_payload_data() = std::move(value.mPayloadData);
  dest.set_page_number(value.mPageNumber);
  dest.set_index(value.mIndex);
}

}
