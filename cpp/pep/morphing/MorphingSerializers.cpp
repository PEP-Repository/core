#include <pep/morphing/MorphingSerializers.hpp>

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <stdexcept>

namespace pep {

MetadataXEntry Serializer<MetadataXEntry>::fromProtocolBuffer(
  proto::MetadataXEntry&& source) const {
  return MetadataXEntry::FromStored(source.payload(), source.encrypted(), source.bound());
}

void Serializer<MetadataXEntry>::moveIntoProtocolBuffer(proto::MetadataXEntry& dest, MetadataXEntry value) const {
  dest.set_payload(std::string(value.payloadForStore()));
  dest.set_bound(value.bound());
  dest.set_encrypted(value.storeEncrypted());
}

Metadata Serializer<Metadata>::fromProtocolBuffer(proto::Metadata&& source) const {
  Metadata result = Metadata(
    std::move(*source.mutable_tag()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    Serialization::FromProtocolBuffer(source.encryption_scheme())
  );

  if (!source.mutable_original_payload_entry_id()->empty()) {
    result.setOriginalPayloadEntryId(std::move(*source.mutable_original_payload_entry_id()));
  }

  // for (auto&& [name, xentrypb] : *source.mutable_extra()) {  // crashes clang
  for (auto&& pair : *source.mutable_extra()) {

    auto name = pair.first;
    auto xentrypb = pair.second;

    [[maybe_unused]] const auto [it, inserted] = result.extra().insert({
        name,
        Serialization::FromProtocolBuffer(std::move(xentrypb))
      });
    assert(inserted);
  }
  return result;
}

void Serializer<Metadata>::moveIntoProtocolBuffer(
  proto::Metadata& dest, Metadata value) const {

  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_timestamp(), value.getBlindingTimestamp());

  if (value.getEncryptionScheme() != EncryptionScheme::V1)
    dest.set_encryption_scheme(
      Serialization::ToProtocolBuffer(value.getEncryptionScheme()));

  *dest.mutable_tag() = value.getTag();

  if (value.getOriginalPayloadEntryId().has_value()) {
    *dest.mutable_original_payload_entry_id() = *value.getOriginalPayloadEntryId();
  }

  for (auto&& [name, xentry] : value.extra()) {
    [[maybe_unused]] const auto [it, inserted] = dest.mutable_extra()->insert({
        name,
        Serialization::ToProtocolBuffer(xentry)
      });
    assert(inserted);
  }
}

}
