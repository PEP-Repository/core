#pragma once

#include <pep/utils/PropertyBasedContainer.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/storagefacility/EntryPayload.hpp>

namespace pep {

using EpochMillis = uint64_t;

class FileStore;

class EntryContent : boost::noncopyable {
public:
  // Flyweight: to save memory we don't store our own metadata strings, but only pointers to strings owned by FileStore.
  // See e.g. the FileStore::makeMetadataEntry method.
  using Metadata = UniqueItemsContainer<const std::string*>::map<const std::string*>;
  using MetadataEntry = Metadata::value_type;

private:
  static constexpr EpochMillis NO_PREVIOUS_PAYLOAD_ENTRY = 0;

  EncryptedKey mPolymorphicKey;
  EpochMillis mBlindingTimestamp;
  EncryptionScheme mEncryptionScheme;
  EpochMillis mOriginalPayloadEntryTimestamp = NO_PREVIOUS_PAYLOAD_ENTRY; // Sentinel value (0) indicates that this content has its own original payload
  Metadata mMetadata; // Does not include "x-" prefixes: see comment in PersistedEntryProperties.hpp
  std::shared_ptr<EntryPayload> mPayload;

public:
  EntryContent(const EncryptedKey& polymorphicKey,
    EpochMillis blindingTimestamp,
    EncryptionScheme encryptionScheme,
    Metadata metadata,
    std::optional<EpochMillis> originalPayloadEntryTimestamp = std::nullopt,
    std::shared_ptr<EntryPayload> payload = nullptr);
  EntryContent(const EntryContent& original, EpochMillis originalEntryValidFrom);

  const EncryptedKey& getPolymorphicKey() const noexcept { return mPolymorphicKey; }
  EpochMillis getBlindingTimestamp() const noexcept { return mBlindingTimestamp; }
  EncryptionScheme getEncryptionScheme() const noexcept { return mEncryptionScheme; }
  std::optional<EpochMillis> getOriginalPayloadEntryTimestamp() const;

  std::shared_ptr<EntryPayload> payload() const noexcept { return mPayload; }
  void setPayload(std::shared_ptr<EntryPayload> payload);

  const Metadata& metadata() const { return mMetadata; }

  static void Save(const std::unique_ptr<EntryContent>& content, PersistedEntryProperties& properties, std::vector<PageId>& pages);
  static std::unique_ptr<EntryContent> Load(FileStore& fileStore, PersistedEntryProperties& properties, std::vector<PageId>& pages);
};

}
