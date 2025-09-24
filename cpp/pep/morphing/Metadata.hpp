#pragma once

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace pep {

// Method by which pages are encrypted and how their metadata is
// cryptographically bound.  Influences encryption/decryption on
// pages, but also (un)blinding of keys.
enum class EncryptionScheme {
  // Old scheme which uses protobuf to serialize metadata and
  // does not bind page numbers.  See issue #566 and #460.
  V1 = 0,

  // New scheme which uses a stable serializer for metadata and
  // binds page numbers.
  V2 = 1,

  // Moves inverse() from unblinding to blinding and binds participant
  // to blinding.  See #719 and #720.
  V3 = 2,

  // Alias for the latest encryption scheme: the one that current code uses to encrypt entries
  LATEST = V3
};

class MetadataXEntry;
using NamedMetadataXEntry = std::pair<const std::string, MetadataXEntry>;

/// Extra metadata entry, may be in encrypted or decrypted form
class MetadataXEntry {
  /// Payload, may be in encrypted or decrypted form according to \ref mIsEncrypted
  std::string mPayload;
  /// Should payload be stored in encrypted form at the server?
  bool mStoreEncrypted = false;
  bool mIsEncrypted = false;
  bool mBound = false;

public:
  [[nodiscard]] static MetadataXEntry FromStored(std::string payload, bool encrypted, bool bound) {
    MetadataXEntry entry;
    entry.mPayload = std::move(payload);
    entry.mIsEncrypted = entry.mStoreEncrypted = encrypted;
    entry.mBound = bound;
    return entry;
  }

  [[nodiscard]] static MetadataXEntry FromPlaintext(std::string plaintext, bool storeEncrypted, bool bound) {
    MetadataXEntry entry;
    entry.mPayload = std::move(plaintext);
    entry.mIsEncrypted = false;
    entry.mStoreEncrypted = storeEncrypted;
    entry.mBound = bound;
    return entry;
  }

  [[nodiscard]] bool storeEncrypted() const { return mStoreEncrypted; }
  [[nodiscard]] bool isEncrypted() const { return mIsEncrypted; }
  [[nodiscard]] bool bound() const { return mBound; }

  /// Return payload for store operations, which may be encrypted.
  /// Requires \ref prepareForStore to be called first
  /// \throws std::runtime_error if payload is not in encrypted form but should be
  [[nodiscard]] const std::string& payloadForStore() const {
    if (mStoreEncrypted && !mIsEncrypted) {
      throw std::runtime_error("Metadata entry is not encrypted yet");
    }
    return mPayload;
  }

  /// Return decrypted payload.
  /// Requires \ref preparePlaintext to be called first
  /// \throws std::runtime_error if payload is not in decrypted form
  [[nodiscard]] const std::string& plaintext() const {
    if (mIsEncrypted) {
      throw std::runtime_error("Metadata entry is not decrypted yet");
    }
    return mPayload;
  }

  /// Returns a copy, but with payload encrypted if required
  [[nodiscard]] MetadataXEntry prepareForStore(const std::string& aeskey) const;
  [[nodiscard]] MetadataXEntry preparePlaintext(const std::string& aeskey) const;

  static NamedMetadataXEntry MakeFileExtension(std::string extension) {
    return { "fileExtension", FromPlaintext(std::move(extension), false, false) };
  }
};

struct KeyBlindingAdditionalData {
  std::string content;

  // Indicates whether the component itself should be used (false) or its
  // inverse (true) for blinding.  (See #719)
  bool invertComponent;
};

class Metadata {
 public:
  inline Metadata(): mBlindingTimestamp(0) {}
  inline Metadata(std::string tag, Timestamp date)
    : mBlindingTimestamp(date), mTag(std::move(tag)) { }
  inline Metadata(std::string tag, Timestamp date, EncryptionScheme scheme)
    : mBlindingTimestamp(date), mTag(std::move(tag)), mEncryptionScheme(scheme) { }

  inline Timestamp getBlindingTimestamp() const {
    return mBlindingTimestamp;
  }
  inline Metadata& setBlindingTimestamp(const Timestamp& date) {
    mBlindingTimestamp = date;
    return *this;
  }
  inline std::string getTag() const {
    return mTag;
  }
  inline Metadata& setTag(const std::string& tag) {
    mTag = tag;
    return *this;
  }
  inline EncryptionScheme getEncryptionScheme() const {
      return mEncryptionScheme;
  }
  inline void setEncryptionScheme(EncryptionScheme scheme) {
      mEncryptionScheme = scheme;
  }
  inline const std::optional<std::string>& getOriginalPayloadEntryId() const {
    return mOriginalPayloadEntryId;
  }
  inline void setOriginalPayloadEntryId(std::string id) {
    mOriginalPayloadEntryId = std::move(id);
  }

  inline std::map<std::string,MetadataXEntry>& extra() {
    return this->mExtra;
  }

  inline const std::map<std::string,MetadataXEntry>& extra() const {
    return this->mExtra;
  }

  Metadata decrypt(const std::string& aeskey) const;

  // Compute the additional data used to bind metadata for the keyblinding.
  KeyBlindingAdditionalData computeKeyBlindingAdditionalData(const LocalPseudonym& localPseudonym) const;

 private:
  Timestamp mBlindingTimestamp;
  std::string mTag;
  EncryptionScheme mEncryptionScheme = EncryptionScheme::V3;
  std::optional<std::string> mOriginalPayloadEntryId;

  // N.B. for a consistent result when blinding the encrypted AES key
  // it is important that mExtra is a sorted std::map.
  std::map<std::string,MetadataXEntry> mExtra;
};

}

