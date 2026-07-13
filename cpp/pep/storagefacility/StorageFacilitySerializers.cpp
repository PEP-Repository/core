#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/serialization/IndexListSerializer.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

namespace pep {

DataEnumerationRequest2 Serializer<DataEnumerationRequest2>::fromProtocolBuffer(proto::DataEnumerationRequest2&& source) const {
  DataEnumerationRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.columns = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.pseudonyms = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataEnumerationRequest2>::moveIntoProtocolBuffer(proto::DataEnumerationRequest2& dest, DataEnumerationRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  if (value.pseudonyms)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.pseudonyms));
  if (value.columns)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.columns));
}

DataEnumerationEntry2 Serializer<DataEnumerationEntry2>::fromProtocolBuffer(proto::DataEnumerationEntry2&& source) const {
  DataEnumerationEntry2 result;
  result.metadata = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.polymorphicKey = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.fileSize = source.file_size();
  result.id = std::move(*source.mutable_id());
  result.columnIndex = source.column_index();
  result.pseudonymIndex = source.pseudonym_index();
  result.index = source.index();
  return result;
}

void Serializer<DataEnumerationEntry2>::moveIntoProtocolBuffer(proto::DataEnumerationEntry2& dest, DataEnumerationEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.metadata));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.polymorphicKey);
  dest.set_file_size(value.fileSize);
  *dest.mutable_id() = std::move(value.id);
  dest.set_column_index(value.columnIndex);
  dest.set_pseudonym_index(value.pseudonymIndex);
  dest.set_index(value.index);
}

DataEnumerationResponse2 Serializer<DataEnumerationResponse2>::fromProtocolBuffer(proto::DataEnumerationResponse2&& source) const {
  DataEnumerationResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  result.hasMore = source.has_more();
  return result;
}

void Serializer<DataEnumerationResponse2>::moveIntoProtocolBuffer(proto::DataEnumerationResponse2& dest, DataEnumerationResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
  dest.set_has_more(value.hasMore);
}

MetadataReadRequest2 Serializer<MetadataReadRequest2>::fromProtocolBuffer(proto::MetadataReadRequest2&& source) const {
  MetadataReadRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.ids.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids.push_back(std::move(x));
  return result;
}

void Serializer<MetadataReadRequest2>::moveIntoProtocolBuffer(proto::MetadataReadRequest2& dest, MetadataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids.size()));
  for (auto x : value.ids)
    dest.add_ids(std::move(x));
}

DataReadRequest2 Serializer<DataReadRequest2>::fromProtocolBuffer(proto::DataReadRequest2&& source) const {
  DataReadRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  result.ids.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids.push_back(std::move(x));
  return result;
}

void Serializer<DataReadRequest2>::moveIntoProtocolBuffer(proto::DataReadRequest2& dest, DataReadRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids.size()));
  for (auto x : value.ids)
    dest.add_ids(std::move(x));
}

MetadataUpdateRequest2 Serializer<MetadataUpdateRequest2>::fromProtocolBuffer(proto::MetadataUpdateRequest2&& source) const {
  MetadataUpdateRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<MetadataUpdateRequest2>::moveIntoProtocolBuffer(proto::MetadataUpdateRequest2& dest, MetadataUpdateRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
}

MetadataUpdateResponse2 Serializer<MetadataUpdateResponse2>::fromProtocolBuffer(proto::MetadataUpdateResponse2&& source) const {
  MetadataUpdateResponse2 result;
  result.ids.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids.push_back(std::move(x));
  return result;
}

void Serializer<MetadataUpdateResponse2>::moveIntoProtocolBuffer(proto::MetadataUpdateResponse2& dest, MetadataUpdateResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids.size()));
  for (auto x : value.ids)
    dest.add_ids(std::move(x));
}

DataStoreResponse2 Serializer<DataStoreResponse2>::fromProtocolBuffer(proto::DataStoreResponse2&& source) const {
  DataStoreResponse2 result;
  result.ids.reserve(static_cast<size_t>(source.ids().size()));
  for (auto& x : *source.mutable_ids())
    result.ids.push_back(std::move(x));
  result.hash = source.hash();
  return result;
}

void Serializer<DataStoreResponse2>::moveIntoProtocolBuffer(proto::DataStoreResponse2& dest, DataStoreResponse2 value) const {
  dest.mutable_ids()->Reserve(static_cast<int>(value.ids.size()));
  for (auto x : value.ids)
    dest.add_ids(std::move(x));
  dest.set_hash(value.hash);
}

DataStoreRequest2 Serializer<DataStoreRequest2>::fromProtocolBuffer(proto::DataStoreRequest2&& source) const {
  DataStoreRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataStoreRequest2>::moveIntoProtocolBuffer(proto::DataStoreRequest2& dest, DataStoreRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
}

DataStoreEntry2 Serializer<DataStoreEntry2>::fromProtocolBuffer(proto::DataStoreEntry2&& source) const {
  DataStoreEntry2 result;
  result.metadata = Serialization::FromProtocolBuffer(std::move(*source.mutable_metadata()));
  result.polymorphicKey = Serialization::FromProtocolBuffer(std::move(*source.mutable_polymorphic_key()));
  result.columnIndex = source.column_index();
  result.pseudonymIndex = source.pseudonym_index();
  return result;
}

void Serializer<DataStoreEntry2>::moveIntoProtocolBuffer(proto::DataStoreEntry2& dest, DataStoreEntry2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_metadata(), std::move(value.metadata));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_polymorphic_key(), value.polymorphicKey);
  dest.set_column_index(value.columnIndex);
  dest.set_pseudonym_index(value.pseudonymIndex);
}

DataDeleteResponse2 Serializer<DataDeleteResponse2>::fromProtocolBuffer(proto::DataDeleteResponse2&& source) const {
  return {
    .timestamp = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .entries = Serialization::FromProtocolBuffer(std::move(*source.mutable_entries())),
  };
}

void Serializer<DataDeleteResponse2>::moveIntoProtocolBuffer(proto::DataDeleteResponse2& dest, DataDeleteResponse2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.timestamp);
}

DataDeleteRequest2 Serializer<DataDeleteRequest2>::fromProtocolBuffer(proto::DataDeleteRequest2&& source) const {
  DataDeleteRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataDeleteRequest2>::moveIntoProtocolBuffer(proto::DataDeleteRequest2& dest, DataDeleteRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
}

DataRequestEntry2 Serializer<DataRequestEntry2>::fromProtocolBuffer(proto::DataRequestEntry2&& source) const {
  DataRequestEntry2 result;
  result.columnIndex = source.column_index();
  result.pseudonymIndex = source.pseudonym_index();
  return result;
}

void Serializer<DataRequestEntry2>::moveIntoProtocolBuffer(proto::DataRequestEntry2& dest, DataRequestEntry2 value) const {
  dest.set_column_index(value.columnIndex);
  dest.set_pseudonym_index(value.pseudonymIndex);
}

DataHistoryRequest2 Serializer<DataHistoryRequest2>::fromProtocolBuffer(proto::DataHistoryRequest2&& source) const {
  DataHistoryRequest2 result;
  result.ticket = Serialization::FromProtocolBuffer(std::move(*source.mutable_ticket()));
  if (source.has_columns())
    result.columns = Serialization::FromProtocolBuffer(std::move(*source.mutable_columns()));
  if (source.has_pseudonyms())
    result.pseudonyms = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonyms()));
  return result;
}

void Serializer<DataHistoryRequest2>::moveIntoProtocolBuffer(proto::DataHistoryRequest2& dest, DataHistoryRequest2 value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_ticket(), std::move(value.ticket));
  if (value.pseudonyms)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonyms(), std::move(*value.pseudonyms));
  if (value.columns)
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_columns(), std::move(*value.columns));
}

DataHistoryEntry2 Serializer<DataHistoryEntry2>::fromProtocolBuffer(proto::DataHistoryEntry2&& source) const {
  return {
    .columnIndex = source.column_index(),
    .pseudonymIndex = source.pseudonym_index(),
    .timestamp = Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    .id = std::move(*source.mutable_id()),
  };
}

void Serializer<DataHistoryEntry2>::moveIntoProtocolBuffer(proto::DataHistoryEntry2& dest, DataHistoryEntry2 value) const {
  dest.set_column_index(value.columnIndex);
  dest.set_pseudonym_index(value.pseudonymIndex);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.timestamp);
  *dest.mutable_id() = std::move(value.id);
}

DataHistoryResponse2 Serializer<DataHistoryResponse2>::fromProtocolBuffer(proto::DataHistoryResponse2&& source) const {
  DataHistoryResponse2 result;
  Serialization::AssignFromRepeatedProtocolBuffer(result.entries,
    std::move(*source.mutable_entries()));
  return result;
}

void Serializer<DataHistoryResponse2>::moveIntoProtocolBuffer(proto::DataHistoryResponse2& dest, DataHistoryResponse2 value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_entries(), std::move(value.entries));
}

DataPayloadPage Serializer<DataPayloadPage>::fromProtocolBuffer(proto::DataPayloadPage&& source) const {
  DataPayloadPage result;

  result.cryptoNonce = std::move(*source.mutable_crypto_nonce());
  result.cryptoMac = std::move(*source.mutable_crypto_mac());
  result.payloadData = std::move(*source.mutable_payload_data());
  result.pageNumber = source.page_number();
  result.index = source.index();

  return result;
}

void Serializer<DataPayloadPage>::moveIntoProtocolBuffer(proto::DataPayloadPage& dest, DataPayloadPage value) const {
  *dest.mutable_crypto_nonce() = std::move(value.cryptoNonce);
  *dest.mutable_crypto_mac() = std::move(value.cryptoMac);
  *dest.mutable_payload_data() = std::move(value.payloadData);
  dest.set_page_number(value.pageNumber);
  dest.set_index(value.index);
}

DataSizeRequest Serializer<DataSizeRequest>::fromProtocolBuffer(proto::DataSizeRequest&& source) const {
  DataSizeRequest result;
  for (const auto& column : source.columns()) {
    if (!result.columns.emplace(column).second) {
      throw std::runtime_error("Can't insert duplicate column '" + column + "' into data size request");
    }
  }
  return result;
}

void Serializer<DataSizeRequest>::moveIntoProtocolBuffer(proto::DataSizeRequest& dest, DataSizeRequest value) const {
  dest.mutable_columns()->Reserve(static_cast<int>(value.columns.size()));
  for (auto& column : value.columns) {
    dest.add_columns(column);
  }
}

DataSizeResponse Serializer<DataSizeResponse>::fromProtocolBuffer(proto::DataSizeResponse&& source) const {
  return DataSizeResponse{
    .blockSize = source.block_size(),
    .totalBlocks = source.total_blocks(),
    .rollingBlocks = source.rolling_blocks(),
  };
}

void Serializer<DataSizeResponse>::moveIntoProtocolBuffer(proto::DataSizeResponse& dest, DataSizeResponse value) const {
  dest.set_block_size(value.blockSize);
  dest.set_total_blocks(value.totalBlocks);
  dest.set_rolling_blocks(value.rollingBlocks);
}

PagePathResponse Serializer<PagePathResponse>::fromProtocolBuffer(proto::PagePathResponse&& source) const {
  PagePathResponse result;
  for (const auto& path : source.paths()) {
    if (!result.paths.emplace(path).second) {
      throw std::runtime_error("Can't insert duplicate path '" + path + "' into page path response");
    }
  }
  return result;
}

void Serializer<PagePathResponse>::moveIntoProtocolBuffer(proto::PagePathResponse& dest, PagePathResponse value) const {
  dest.mutable_paths()->Reserve(static_cast<int>(value.paths.size()));
  for (auto& path : value.paths) {
    dest.add_paths(path);
  }
}

}
