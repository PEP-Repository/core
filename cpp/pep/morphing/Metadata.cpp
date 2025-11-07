#include <pep/morphing/Metadata.hpp>

#include <pep/crypto/BytesSerializer.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Sha.hpp>

namespace pep {

Metadata Metadata::decrypt(const std::string& aeskey) const {
  Metadata result = *this;

  for (auto&& [name, xentry] : result.mExtra) {
    xentry = xentry.preparePlaintext(aeskey);
  }

  return result;
}

Metadata Metadata::getBound() const {
  using namespace std::ranges;
  Metadata result;
  result.mBlindingTimestamp = mBlindingTimestamp;
  result.mTag = mTag;
  result.mEncryptionScheme = mEncryptionScheme;
  result.mExtra = RangeToCollection<std::map<std::string, MetadataXEntry>>(mExtra
      | views::filter([](const std::pair<const std::string, MetadataXEntry>& entry) { return entry.second.bound(); }));
  return result;
}

KeyBlindingAdditionalData Metadata::computeKeyBlindingAdditionalData(const LocalPseudonym& localPseudonym) const {
  auto scheme = this->getEncryptionScheme();
  if (scheme == EncryptionScheme::V1) {
    // V1 uses Protobuf serialization, which is not guaranteed to be stable
    return {
      Sha256().digest(
        Serialization::ToString(localPseudonym.getValidCurvePoint(), false) +
        Serialization::ToString(*this, false)),
      false };
  }

  if (scheme == EncryptionScheme::V2) {
    std::ostringstream ss;
    ss << PackUint64BE(ToUnderlying(EncryptionScheme::V2));
    ss << PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(this->getBlindingTimestamp())));
    ss << PackUint64BE(this->getTag().size());
    ss << this->getTag();
    return { ss.str(), false };
  }

  if (scheme == EncryptionScheme::V3) {
    std::ostringstream ss;
    ss << PackUint64BE(ToUnderlying(EncryptionScheme::V3));
    ss << PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(this->getBlindingTimestamp())));
    ss << PackUint64BE(this->getTag().size());
    ss << this->getTag();
    ss << localPseudonym.pack();

    // For backwards compatibility nothing more should be added to ss when
    // there are no bound extra entries.
    //
    // Note that it is important for consistency that metadata.extra()
    // is a sorted map.
    for (auto&& [name, xentry] : this->extra()) {
      if (!xentry.bound())
        continue;
      ss << PackUint64BE(name.size());
      ss << name;
      ss << PackUint64BE(xentry.payloadForStore().size());
      ss << xentry.payloadForStore();
      ss << PackUint8(xentry.storeEncrypted());
    }

    return { ss.str(), true };
  }

  throw std::runtime_error("Unknown blinding encryption scheme");

}

MetadataXEntry MetadataXEntry::preparePlaintext(const std::string& aeskey) const {
  MetadataXEntry result = *this;

  if (result.mIsEncrypted) {
    result.mPayload = Serialization::FromString<EncryptedBytes>(result.mPayload, false)
        .decrypt(aeskey).mData;
    result.mIsEncrypted = false;
  }

  return result;
}

MetadataXEntry MetadataXEntry::prepareForStore(const std::string& aeskey) const {
  MetadataXEntry result = *this;

  // Only encrypt if desired
  if (result.mStoreEncrypted && !result.mIsEncrypted) {
    result.mPayload = Serialization::ToString(
        EncryptedBytes(aeskey, Bytes(std::move(result.mPayload))), false);
    result.mIsEncrypted = true;
  }

  return result;
}



}
