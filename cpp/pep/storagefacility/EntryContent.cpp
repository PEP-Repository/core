#include <pep/storagefacility/FileStore.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/utils/MiscUtil.hpp>

namespace pep {

namespace {

const std::string X_ENTRY_PREFIX = "x-";
const std::string POLYMORPHIC_KEY_KEY = "polymorphic-key";
const std::string BLINDING_TIMESTAMP_KEY = "blinding-timestamp";
const std::string ENCRYPTION_SCHEME_KEY = "encryption-scheme";
const std::string ORIGINAL_PAYLOAD_TIMESTAMP_KEY = "original-payload-timestamp";

} // anonymous namespace

EntryContent::EntryContent( Metadata metadata, PayloadData payload, std::optional<Timestamp> originalPayloadEntryTimestamp)
  : mMetadata(std::move(metadata)),
  mPayload(std::move(payload)) {
  if (originalPayloadEntryTimestamp.has_value()) {
    assert(*originalPayloadEntryTimestamp != NO_PREVIOUS_PAYLOAD_ENTRY);
    mOriginalPayloadEntryTimestamp = *originalPayloadEntryTimestamp;
  }
}

EntryContent::PayloadData& EntryContent::PayloadData::operator= (const PayloadData& src) {
  if (&src != this) {
    encryption = src.encryption;
    ptr = src.ptr->clone();
  }
  return *this;
}

EntryContent::EntryContent(const EntryContent& other, Timestamp originalEntryValidFrom)
  : EntryContent(other.mMetadata,
      other.mPayload,
      other.getOriginalPayloadEntryTimestamp().value_or(originalEntryValidFrom)) {}

std::optional<Timestamp> EntryContent::getOriginalPayloadEntryTimestamp() const {
  if (mOriginalPayloadEntryTimestamp == NO_PREVIOUS_PAYLOAD_ENTRY) {
    return std::nullopt;
  }
  return mOriginalPayloadEntryTimestamp;
}

void EntryContent::setPayload(std::shared_ptr<EntryPayload> payload) {
  assert(mPayload.ptr == nullptr);
  mPayload.ptr = payload;
  mOriginalPayloadEntryTimestamp = NO_PREVIOUS_PAYLOAD_ENTRY;
}

void EntryContent::Save(const std::unique_ptr<EntryContent>& content, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  std::shared_ptr<EntryPayload> payload;

  if (content != nullptr) {
    SetPersistedEntryProperty(properties, POLYMORPHIC_KEY_KEY, content->getPolymorphicKey());
    SetPersistedEntryProperty(properties, BLINDING_TIMESTAMP_KEY, static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(content->getBlindingTimestamp())));
    SetPersistedEntryProperty(properties, ENCRYPTION_SCHEME_KEY, content->getEncryptionScheme());
    auto original = content->getOriginalPayloadEntryTimestamp();
    if (original.has_value()) {
      SetPersistedEntryProperty(properties, ORIGINAL_PAYLOAD_TIMESTAMP_KEY, static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(*original)));
    }

    std::transform(content->mMetadata.cbegin(), content->mMetadata.cend(), std::inserter(properties, properties.end()), [](const auto& entry) {
      auto key = X_ENTRY_PREFIX + *entry.first;
      return std::make_pair(key, *entry.second);
      });
    payload = content->mPayload.ptr;
  }

  // Backward compatible: save (absent/empty) payload properties even if there's no content
  EntryPayload::Save(payload, properties, pages);
}

std::unique_ptr<EntryContent> EntryContent::Load(FileStore& fileStore, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  auto polymorphicKey = TryExtractPersistedEntryProperty<EncryptedKey>(properties, POLYMORPHIC_KEY_KEY);
  auto blindingTimestamp =
    GetOptionalValue(
      TryExtractPersistedEntryProperty<std::uint64_t>(properties, BLINDING_TIMESTAMP_KEY),
      [](std::uint64_t ms) { return Timestamp(std::chrono::milliseconds{ms}); });
  auto encryptionScheme = TryExtractPersistedEntryProperty<EncryptionScheme>(properties, ENCRYPTION_SCHEME_KEY);

  assert(polymorphicKey.has_value() == blindingTimestamp.has_value());
  assert(polymorphicKey.has_value() == encryptionScheme.has_value());

  if (!polymorphicKey.has_value()) {
    return nullptr;
  }

  auto originalPayloadTimestamp =
    GetOptionalValue(
          TryExtractPersistedEntryProperty<uint64_t>(properties, ORIGINAL_PAYLOAD_TIMESTAMP_KEY),
          [](std::uint64_t ms) { return Timestamp(std::chrono::milliseconds{ms}); });
  auto payload = EntryPayload::Load(properties, pages);
  assert(pages.empty());

  Metadata storableMetadata;
  std::transform(properties.cbegin(), properties.cend(), std::inserter(storableMetadata, storableMetadata.begin()), [&fileStore](const auto& entry) {
    assert(entry.first.starts_with(X_ENTRY_PREFIX));
    return fileStore.makeMetadataEntry(entry.first.substr(X_ENTRY_PREFIX.size()), entry.second);
    });

  return std::make_unique<EntryContent>(
    storableMetadata,
    PayloadData({.polymorphicKey = *polymorphicKey, .blindingTimestamp = *blindingTimestamp, .scheme = *encryptionScheme}, payload),
    originalPayloadTimestamp);
}

}
