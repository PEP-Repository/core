#pragma once

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/utils/Timestamp.hpp>

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
  Latest = V3
};

class MetadataXEntry;
using NamedMetadataXEntry = std::pair<const std::string, MetadataXEntry>;

/// Extra metadata entry, may be in encrypted or decrypted form
class MetadataXEntry {
  /// Payload, may be in encrypted or decrypted form according to \ref isEncrypted_
  std::string payload_;
  /// Should payload be stored in encrypted form at the server?
  bool storeEncrypted_ = false;
  bool isEncrypted_ = false;
  bool bound_ = false;

public:
  [[nodiscard]] static MetadataXEntry FromStored(std::string payload, bool encrypted, bool bound) {
    MetadataXEntry entry;
    entry.payload_ = std::move(payload);
    entry.isEncrypted_ = entry.storeEncrypted_ = encrypted;
    entry.bound_ = bound;
    return entry;
  }

  [[nodiscard]] static MetadataXEntry FromPlaintext(std::string plaintext, bool storeEncrypted, bool bound) {
    MetadataXEntry entry;
    entry.payload_ = std::move(plaintext);
    entry.isEncrypted_ = false;
    entry.storeEncrypted_ = storeEncrypted;
    entry.bound_ = bound;
    return entry;
  }

  [[nodiscard]] bool storeEncrypted() const { return storeEncrypted_; }
  [[nodiscard]] bool isEncrypted() const { return isEncrypted_; }
  [[nodiscard]] bool bound() const { return bound_; }

  /// Return payload for store operations, which may be encrypted.
  /// Requires \ref prepareForStore to be called first
  /// \throws std::runtime_error if payload is not in encrypted form but should be
  [[nodiscard]] const std::string& payloadForStore() const {
    if (storeEncrypted_ && !isEncrypted_) {
      throw std::runtime_error("Metadata entry is not encrypted yet");
    }
    return payload_;
  }

  /// Return decrypted payload.
  /// Requires \ref preparePlaintext to be called first
  /// \throws std::runtime_error if payload is not in decrypted form
  [[nodiscard]] const std::string& plaintext() const {
    if (isEncrypted_) {
      throw std::runtime_error("Metadata entry is not decrypted yet");
    }
    return payload_;
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
  inline Metadata(): blindingTimestamp_{/*zero*/} {}
  inline Metadata(std::string tag, Timestamp date)
    : blindingTimestamp_(date), tag_(std::move(tag)) { }
  inline Metadata(std::string tag, Timestamp date, EncryptionScheme scheme)
    : blindingTimestamp_(date), tag_(std::move(tag)), encryptionScheme_(scheme) { }

  inline Timestamp getBlindingTimestamp() const {
    return blindingTimestamp_;
  }
  inline Metadata& setBlindingTimestamp(const Timestamp& date) {
    blindingTimestamp_ = date;
    return *this;
  }
  inline std::string getTag() const {
    return tag_;
  }
  inline Metadata& setTag(const std::string& tag) {
    tag_ = tag;
    return *this;
  }
  inline EncryptionScheme getEncryptionScheme() const {
      return encryptionScheme_;
  }
  inline void setEncryptionScheme(EncryptionScheme scheme) {
      encryptionScheme_ = scheme;
  }
  inline const std::optional<std::string>& getOriginalPayloadEntryId() const {
    return originalPayloadEntryId_;
  }
  inline void setOriginalPayloadEntryId(std::string id) {
    originalPayloadEntryId_ = std::move(id);
  }

  inline std::map<std::string,MetadataXEntry>& extra() {
    return this->extra_;
  }

  inline const std::map<std::string,MetadataXEntry>& extra() const {
    return this->extra_;
  }

  Metadata decrypt(const std::string& aeskey) const;

  /// Filter to entries used by \c computeKeyBlindingAdditionalData
  Metadata getBound() const;

  // Compute the additional data used to bind metadata for the keyblinding.
  KeyBlindingAdditionalData computeKeyBlindingAdditionalData(const LocalPseudonym& localPseudonym) const;

 private:
  Timestamp blindingTimestamp_;
  std::string tag_;
  EncryptionScheme encryptionScheme_ = EncryptionScheme::V3;
  std::optional<std::string> originalPayloadEntryId_;

  // N.B. for a consistent result when blinding the encrypted AES key
  // it is important that extra_ is a sorted std::map.
  std::map<std::string,MetadataXEntry> extra_;

  void checkFieldsConsistentWithVersion() const;
};

}

