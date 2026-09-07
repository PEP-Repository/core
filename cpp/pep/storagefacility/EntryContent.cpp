#include <pep/storagefacility/FileStore.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

namespace {

const std::string XEntryPrefix = "x-";
const std::string PolymorphicKeyKey = "polymorphic-key";
const std::string BlindingTimestampKey = "blinding-timestamp";
const std::string EncryptionSchemeKey = "encryption-scheme";
const std::string OriginalPayloadTimestampKey = "original-payload-timestamp";

} // anonymous namespace

EntryContent::EntryContent( Metadata metadata, PayloadData payload, std::optional<Timestamp> originalPayloadEntryTimestamp)
  : metadata_(std::move(metadata)),
  payload_(std::move(payload)) {
  if (originalPayloadEntryTimestamp.has_value()) {
    assert(*originalPayloadEntryTimestamp != NoPreviousPayloadEntry);
    originalPayloadEntryTimestamp_ = *originalPayloadEntryTimestamp;
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
  : EntryContent(other.metadata_,
      other.payload_,
      other.getOriginalPayloadEntryTimestamp().value_or(originalEntryValidFrom)) {}

std::optional<Timestamp> EntryContent::getOriginalPayloadEntryTimestamp() const {
  if (originalPayloadEntryTimestamp_ == NoPreviousPayloadEntry) {
    return std::nullopt;
  }
  return originalPayloadEntryTimestamp_;
}

void EntryContent::setPayload(std::shared_ptr<EntryPayload> payload) {
  assert(payload_.ptr == nullptr);
  payload_.ptr = payload;
  originalPayloadEntryTimestamp_ = NoPreviousPayloadEntry;
}

void EntryContent::Save(const std::unique_ptr<EntryContent>& content, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  std::shared_ptr<EntryPayload> payload;

  if (content != nullptr) {
    SetPersistedEntryProperty(properties, PolymorphicKeyKey, content->getPolymorphicKey());
    SetPersistedEntryProperty(properties, BlindingTimestampKey, content->getBlindingTimestamp());
    SetPersistedEntryProperty(properties, EncryptionSchemeKey, content->getEncryptionScheme());
    auto original = content->getOriginalPayloadEntryTimestamp();
    if (original.has_value()) {
      SetPersistedEntryProperty(properties, OriginalPayloadTimestampKey, *original);
    }

    std::transform(content->metadata_.cbegin(), content->metadata_.cend(), std::inserter(properties, properties.end()), [](const auto& entry) {
      auto key = XEntryPrefix + *entry.first;
      return std::make_pair(key, *entry.second);
      });
    payload = content->payload_.ptr;
  }

  // Backward compatible: save (absent/empty) payload properties even if there's no content
  EntryPayload::Save(payload, properties, pages);
}

std::unique_ptr<EntryContent> EntryContent::Load(FileStore& fileStore, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  auto polymorphicKey = TryExtractPersistedEntryProperty<EncryptedKey>(properties, PolymorphicKeyKey);
  auto blindingTimestamp = TryExtractPersistedEntryProperty<Timestamp>(properties, BlindingTimestampKey);
  auto encryptionScheme = TryExtractPersistedEntryProperty<EncryptionScheme>(properties, EncryptionSchemeKey);

  assert(polymorphicKey.has_value() == blindingTimestamp.has_value());
  assert(polymorphicKey.has_value() == encryptionScheme.has_value());

  if (!polymorphicKey.has_value()) {
    return nullptr;
  }

  auto originalPayloadTimestamp = TryExtractPersistedEntryProperty<Timestamp>(properties, OriginalPayloadTimestampKey);
  auto payload = EntryPayload::Load(properties, pages);
  assert(pages.empty());

  Metadata storableMetadata;
  std::transform(properties.cbegin(), properties.cend(), std::inserter(storableMetadata, storableMetadata.begin()), [&fileStore](const auto& entry) {
    assert(entry.first.starts_with(XEntryPrefix));
    return fileStore.makeMetadataEntry(entry.first.substr(XEntryPrefix.size()), entry.second);
    });

  return std::make_unique<EntryContent>(
    storableMetadata,
    PayloadData({.polymorphicKey = *polymorphicKey, .blindingTimestamp = *blindingTimestamp, .scheme = *encryptionScheme}, payload),
    originalPayloadTimestamp);
}

}
