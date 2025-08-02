#include <pep/storagefacility/FileStore.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

namespace {

const std::string X_ENTRY_PREFIX = "x-";
const std::string POLYMORPHIC_KEY_KEY = "polymorphic-key";
const std::string BLINDING_TIMESTAMP_KEY = "blinding-timestamp";
const std::string ENCRYPTION_SCHEME_KEY = "encryption-scheme";
const std::string ORIGINAL_PAYLOAD_TIMESTAMP_KEY = "original-payload-timestamp";

}

EntryContent::EntryContent(const EncryptedKey& polymorphicKey,
  EpochMillis blindingTimestamp,
  EncryptionScheme encryptionScheme,
  Metadata metadata,
  std::optional<EpochMillis> originalPayloadEntryTimestamp,
  std::shared_ptr<EntryPayload> payload)
  : mPolymorphicKey(polymorphicKey),
  mBlindingTimestamp(blindingTimestamp),
  mEncryptionScheme(encryptionScheme),
  mMetadata(std::move(metadata)),
  mPayload(std::move(payload)) {
  if (originalPayloadEntryTimestamp.has_value()) {
    assert(*originalPayloadEntryTimestamp != NO_PREVIOUS_PAYLOAD_ENTRY);
    mOriginalPayloadEntryTimestamp = *originalPayloadEntryTimestamp;
  }
}

EntryContent::EntryContent(const EntryContent& other, EpochMillis originalEntryValidFrom)
  : EntryContent(other.mPolymorphicKey, other.mBlindingTimestamp, other.mEncryptionScheme, other.mMetadata,
    other.getOriginalPayloadEntryTimestamp().value_or(originalEntryValidFrom),
    other.payload()->clone()) {
}

std::optional<EpochMillis> EntryContent::getOriginalPayloadEntryTimestamp() const {
  if (mOriginalPayloadEntryTimestamp == NO_PREVIOUS_PAYLOAD_ENTRY) {
    return std::nullopt;
  }
  return mOriginalPayloadEntryTimestamp;
}

void EntryContent::setPayload(std::shared_ptr<EntryPayload> payload) {
  assert(mPayload == nullptr);
  mPayload = payload;
  mOriginalPayloadEntryTimestamp = NO_PREVIOUS_PAYLOAD_ENTRY;
}

void EntryContent::Save(const std::unique_ptr<EntryContent>& content, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  std::shared_ptr<EntryPayload> payload;

  if (content != nullptr) {
    SetPersistedEntryProperty(properties, POLYMORPHIC_KEY_KEY, content->getPolymorphicKey());
    SetPersistedEntryProperty(properties, BLINDING_TIMESTAMP_KEY, content->getBlindingTimestamp());
    SetPersistedEntryProperty(properties, ENCRYPTION_SCHEME_KEY, content->getEncryptionScheme());
    auto original = content->getOriginalPayloadEntryTimestamp();
    if (original.has_value()) {
      SetPersistedEntryProperty(properties, ORIGINAL_PAYLOAD_TIMESTAMP_KEY, *original);
    }

    std::transform(content->mMetadata.cbegin(), content->mMetadata.cend(), std::inserter(properties, properties.end()), [](const auto& entry) {
      auto key = X_ENTRY_PREFIX + *entry.first;
      return std::make_pair(key, *entry.second);
      });
    payload = content->mPayload;
  }

  // Backward compatible: save (absent/empty) payload properties even if there's no content
  EntryPayload::Save(payload, properties, pages);
}

std::unique_ptr<EntryContent> EntryContent::Load(FileStore& fileStore, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  auto polymorphicKey = TryExtractPersistedEntryProperty<EncryptedKey>(properties, POLYMORPHIC_KEY_KEY);
  auto blindingTimestamp = TryExtractPersistedEntryProperty<EpochMillis>(properties, BLINDING_TIMESTAMP_KEY);
  auto encryptionScheme = TryExtractPersistedEntryProperty<EncryptionScheme>(properties, ENCRYPTION_SCHEME_KEY);

  assert(polymorphicKey.has_value() == blindingTimestamp.has_value());
  assert(polymorphicKey.has_value() == encryptionScheme.has_value());
  if (!polymorphicKey.has_value()) {
    return nullptr;
  }

  auto originalPayloadTimestamp = TryExtractPersistedEntryProperty<uint64_t>(properties, ORIGINAL_PAYLOAD_TIMESTAMP_KEY);
  auto payload = EntryPayload::Load(properties, pages);
  assert(pages.empty());

  Metadata storableMetadata;
  std::transform(properties.cbegin(), properties.cend(), std::inserter(storableMetadata, storableMetadata.begin()), [&fileStore](const auto& entry) {
    assert(entry.first.starts_with(X_ENTRY_PREFIX));
    return fileStore.makeMetadataEntry(entry.first.substr(X_ENTRY_PREFIX.size()), entry.second);
    });

  return std::make_unique<EntryContent>(*polymorphicKey, *blindingTimestamp, *encryptionScheme, storableMetadata, originalPayloadTimestamp, payload);
}

}
