#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/serialization/IndexListSerializer.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

namespace pep {

DataEnumerationRequest2 Serializer<DataEnumerationRequest2>::fromProtocolBuffer(proto::DataEnumerationRequest2&& source) const {
  DataEnumerationRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.columns_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.pseudonyms_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataEnumerationRequest2>::moveIntoProtocolBuffer(proto::DataEnumerationRequest2& dest, DataEnumerationRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  if (value.pseudonyms_)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.pseudonyms_));
  if (value.columns_)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.columns_));
}

DataEnumerationEntry2 Serializer<DataEnumerationEntry2>::fromProtocolBuffer(proto::DataEnumerationEntry2&& source) const {
  DataEnumerationEntry2 result;
  result.metadata_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.polymorphicKey_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.fileSize_ = source.file_size();
  result.id_ = std::move(*source.mutable_id());
  result.columnIndex_ = source.column_index();
  result.pseudonymIndex_ = source.pseudonym_index();
  result.index_ = source.index();
  return result;
}

void Serializer<DataEnumerationEntry2>::moveIntoProtocolBuffer(proto::DataEnumerationEntry2& dest, DataEnumerationEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.metadata_));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.polymorphicKey_);
  dest.set_file_size(value.fileSize_);
  *dest.mutable_id() = std::move(value.id_);
  dest.set_column_index(value.columnIndex_);
  dest.set_pseudonym_index(value.pseudonymIndex_);
  dest.set_index(value.index_);
}

DataEnumerationResponse2 Serializer<DataEnumerationResponse2>::fromProtocolBuffer(proto::DataEnumerationResponse2&& source) const {
  DataEnumerationResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries_,
    std::move(*source.mutable_entries()));
  result.hasMore_ = source.has_more();
  return result;
}

void Serializer<DataEnumerationResponse2>::moveIntoProtocolBuffer(proto::DataEnumerationResponse2& dest, DataEnumerationResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
  dest.set_has_more(value.hasMore_);
}

MetadataReadRequest2 Serializer<MetadataReadRequest2>::fromProtocolBuffer(proto::MetadataReadRequest2&& source) const {
  MetadataReadRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.ids_.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids_.push_back(std::move(x));
  return result;
}

void Serializer<MetadataReadRequest2>::moveIntoProtocolBuffer(proto::MetadataReadRequest2& dest, MetadataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids_.size()));
  for (auto x : value.ids_)
    dest.add_ids(std::move(x));
}

DataReadRequest2 Serializer<DataReadRequest2>::fromProtocolBuffer(proto::DataReadRequest2&& source) const {
  DataReadRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.ids_.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids_.push_back(std::move(x));
  return result;
}

void Serializer<DataReadRequest2>::moveIntoProtocolBuffer(proto::DataReadRequest2& dest, DataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids_.size()));
  for (auto x : value.ids_)
    dest.add_ids(std::move(x));
}

MetadataUpdateRequest2 Serializer<MetadataUpdateRequest2>::fromProtocolBuffer(proto::MetadataUpdateRequest2&& source) const {
  MetadataUpdateRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries_,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<MetadataUpdateRequest2>::moveIntoProtocolBuffer(proto::MetadataUpdateRequest2& dest, MetadataUpdateRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
}

MetadataUpdateResponse2 Serializer<MetadataUpdateResponse2>::fromProtocolBuffer(proto::MetadataUpdateResponse2&& source) const {
  MetadataUpdateResponse2 result;
  result.ids_.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids_.push_back(std::move(x));
  return result;
}

void Serializer<MetadataUpdateResponse2>::moveIntoProtocolBuffer(proto::MetadataUpdateResponse2& dest, MetadataUpdateResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids_.size()));
  for (auto x : value.ids_)
    dest.add_ids(std::move(x));
}

DataStoreResponse2 Serializer<DataStoreResponse2>::fromProtocolBuffer(proto::DataStoreResponse2&& source) const {
  DataStoreResponse2 result;
  result.ids_.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids_.push_back(std::move(x));
  result.hash_ = source.hash();
  return result;
}

void Serializer<DataStoreResponse2>::moveIntoProtocolBuffer(proto::DataStoreResponse2& dest, DataStoreResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids_.size()));
  for (auto x : value.ids_)
    dest.add_ids(std::move(x));
  dest.set_hash(value.hash_);
}

DataStoreRequest2 Serializer<DataStoreRequest2>::fromProtocolBuffer(proto::DataStoreRequest2&& source) const {
  DataStoreRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries_,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataStoreRequest2>::moveIntoProtocolBuffer(proto::DataStoreRequest2& dest, DataStoreRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
}

DataStoreEntry2 Serializer<DataStoreEntry2>::fromProtocolBuffer(proto::DataStoreEntry2&& source) const {
  DataStoreEntry2 result;
  result.metadata_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.polymorphicKey_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.columnIndex_ = source.column_index();
  result.pseudonymIndex_ = source.pseudonym_index();
  return result;
}

void Serializer<DataStoreEntry2>::moveIntoProtocolBuffer(proto::DataStoreEntry2& dest, DataStoreEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.metadata_));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.polymorphicKey_);
  dest.set_column_index(value.columnIndex_);
  dest.set_pseudonym_index(value.pseudonymIndex_);
}

DataDeleteResponse2 Serializer<DataDeleteResponse2>::fromProtocolBuffer(proto::DataDeleteResponse2&& source) const {
  return {
    .timestamp_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .entries_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_entries())),
  };
}

void Serializer<DataDeleteResponse2>::moveIntoProtocolBuffer(proto::DataDeleteResponse2& dest, DataDeleteResponse2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.timestamp_);
}

DataDeleteRequest2 Serializer<DataDeleteRequest2>::fromProtocolBuffer(proto::DataDeleteRequest2&& source) const {
  DataDeleteRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries_,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataDeleteRequest2>::moveIntoProtocolBuffer(proto::DataDeleteRequest2& dest, DataDeleteRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
}

DataRequestEntry2 Serializer<DataRequestEntry2>::fromProtocolBuffer(proto::DataRequestEntry2&& source) const {
  DataRequestEntry2 result;
  result.columnIndex_ = source.column_index();
  result.pseudonymIndex_ = source.pseudonym_index();
  return result;
}

void Serializer<DataRequestEntry2>::moveIntoProtocolBuffer(proto::DataRequestEntry2& dest, DataRequestEntry2 value) const {
  dest.set_column_index(value.columnIndex_);
  dest.set_pseudonym_index(value.pseudonymIndex_);
}

DataHistoryRequest2 Serializer<DataHistoryRequest2>::fromProtocolBuffer(proto::DataHistoryRequest2&& source) const {
  DataHistoryRequest2 result;
  result.ticket_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.columns_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.pseudonyms_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataHistoryRequest2>::moveIntoProtocolBuffer(proto::DataHistoryRequest2& dest, DataHistoryRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket_));
  if (value.pseudonyms_)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.pseudonyms_));
  if (value.columns_)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.columns_));
}

DataHistoryEntry2 Serializer<DataHistoryEntry2>::fromProtocolBuffer(proto::DataHistoryEntry2&& source) const {
  return {
    .columnIndex_ = source.column_index(),
    .pseudonymIndex_ = source.pseudonym_index(),
    .timestamp_ = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .id_ = std::move(*source.mutable_id()),
  };
}

void Serializer<DataHistoryEntry2>::moveIntoProtocolBuffer(proto::DataHistoryEntry2& dest, DataHistoryEntry2 value) const {
  dest.set_column_index(value.columnIndex_);
  dest.set_pseudonym_index(value.pseudonymIndex_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.timestamp_);
  *dest.mutable_id() = std::move(value.id_);
}

DataHistoryResponse2 Serializer<DataHistoryResponse2>::fromProtocolBuffer(proto::DataHistoryResponse2&& source) const {
  DataHistoryResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries_,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataHistoryResponse2>::moveIntoProtocolBuffer(proto::DataHistoryResponse2& dest, DataHistoryResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries_));
}

DataPayloadPage Serializer<DataPayloadPage>::fromProtocolBuffer(proto::DataPayloadPage&& source) const {
  DataPayloadPage result;

  result.cryptoNonce_ = std::move(*source.mutable_crypto_nonce());
  result.cryptoMac_ = std::move(*source.mutable_crypto_mac());
  result.payloadData_ = std::move(*source.mutable_payload_data());
  result.pageNumber_ = source.page_number();
  result.index_ = source.index();

  return result;
}

void Serializer<DataPayloadPage>::moveIntoProtocolBuffer(proto::DataPayloadPage& dest, DataPayloadPage value) const {
  *dest.mutable_crypto_nonce() = std::move(value.cryptoNonce_);
  *dest.mutable_crypto_mac() = std::move(value.cryptoMac_);
  *dest.mutable_payload_data() = std::move(value.payloadData_);
  dest.set_page_number(value.pageNumber_);
  dest.set_index(value.index_);
}

DataSizeRequest Serializer<DataSizeRequest>::fromProtocolBuffer(proto::DataSizeRequest&& source) const {
  DataSizeRequest result;
  for (const auto& column : source.columns()) {
    if (!result.columns_.emplace(column).second) {
      throw std::runtime_error("Can't insert duplicate column '" + column + "' into data size request");
    }
  }
  return result;
}

void Serializer<DataSizeRequest>::moveIntoProtocolBuffer(proto::DataSizeRequest& dest, DataSizeRequest value) const {
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns_.size()));
  for (auto& column : value.columns_) {
    dest.add_columns(column);
  }
}

DataSizeResponse Serializer<DataSizeResponse>::fromProtocolBuffer(proto::DataSizeResponse&& source) const {
  return DataSizeResponse{
    .blockSize_ = source.block_size(),
    .totalBlocks_ = source.total_blocks(),
    .rollingBlocks_ = source.rolling_blocks(),
  };
}

void Serializer<DataSizeResponse>::moveIntoProtocolBuffer(proto::DataSizeResponse& dest, DataSizeResponse value) const {
  dest.set_block_size(value.blockSize_);
  dest.set_total_blocks(value.totalBlocks_);
  dest.set_rolling_blocks(value.rollingBlocks_);
}


}
