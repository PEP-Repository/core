#pragma once

#include <pep/utils/PropertyBasedContainer.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/storagefacility/EntryPayload.hpp>

namespace pep {

class FileStore;

class EntryContent : boost::noncopyable {
public:
  // Flyweight: to save memory we don't store our own metadata strings, but only pointers to strings owned by FileStore.
  // See e.g. the FileStore::makeMetadataEntry method.
  using Metadata = UniqueItemsContainer<const std::string*>::map<const std::string*>;
  using MetadataEntry = Metadata::value_type;

  struct PayloadData final {
    struct Encryption final {
      EncryptedKey polymorphicKey;
      Timestamp blindingTimestamp;
      EncryptionScheme scheme;
    };

    /// Fully specified constructor
    PayloadData(Encryption encryption, std::shared_ptr<EntryPayload> ptr): encryption{encryption}, ptr{ptr} {}

    PayloadData(PayloadData&&) = default;
    PayloadData& operator= (PayloadData&&) = default;

    /// Makes a deep copy of all fields
    PayloadData(const PayloadData& src): encryption(src.encryption), ptr(src.ptr->clone()) {}
    /// Makes a deep copy of all fields
    PayloadData& operator= (const PayloadData&);

    Encryption encryption;
    std::shared_ptr<EntryPayload> ptr;
  };

private:
  static constexpr Timestamp NO_PREVIOUS_PAYLOAD_ENTRY = Timestamp::zero();

  Timestamp mOriginalPayloadEntryTimestamp = NO_PREVIOUS_PAYLOAD_ENTRY; // Sentinel value (0) indicates that this content has its own original payload
  Metadata mMetadata; // Does not include "x-" prefixes: see comment in PersistedEntryProperties.hpp
  PayloadData mPayload;

public:
  EntryContent(Metadata metadata,
    PayloadData payload,
    std::optional<Timestamp> originalPayloadEntryTimestamp = std::nullopt);
  EntryContent(const EntryContent& original, Timestamp originalEntryValidFrom);

  const EncryptedKey& getPolymorphicKey() const noexcept { return mPayload.encryption.polymorphicKey; }
  Timestamp getBlindingTimestamp() const noexcept { return mPayload.encryption.blindingTimestamp; }
  EncryptionScheme getEncryptionScheme() const noexcept { return mPayload.encryption.scheme; }

  std::optional<Timestamp> getOriginalPayloadEntryTimestamp() const;

  std::shared_ptr<EntryPayload> payload() const noexcept { return mPayload.ptr; }
  void setPayload(std::shared_ptr<EntryPayload> payload);

  const Metadata& metadata() const { return mMetadata; }

  static void Save(const std::unique_ptr<EntryContent>& content, PersistedEntryProperties& properties, std::vector<PageId>& pages);
  static std::unique_ptr<EntryContent> Load(FileStore& fileStore, PersistedEntryProperties& properties, std::vector<PageId>& pages);
};

}
