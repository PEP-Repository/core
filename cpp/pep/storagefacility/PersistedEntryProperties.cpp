#include <pep/utils/Bitpacking.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>

namespace pep {

template <>
std::string ExtractPersistedEntryProperty<std::string>(PersistedEntryProperties& source, const std::string& key) {
  auto pos = source.find(key);
  if (pos == source.end()) {
    throw std::runtime_error("Metadata does not contain an entry for key " + key);
  }
  auto result = pos->second;
  source.erase(pos);
  return result;
}

template <>
void SetPersistedEntryProperty<std::string>(PersistedEntryProperties& destination, const std::string& key, const std::string& value) {
  destination[key] = value;
}

template <>
uint8_t ExtractPersistedEntryProperty<uint8_t>(PersistedEntryProperties& source, const std::string& key) {
  auto raw = ExtractPersistedEntryProperty<std::string>(source, key);
  if (raw.size() != 1U) {
    throw std::runtime_error("Metadata with key " + key + " does not contain an unsigned 8-bit integer");
  }
  return static_cast<uint8_t>(raw[0]);
}

template <>
void SetPersistedEntryProperty<uint8_t>(PersistedEntryProperties& destination, const std::string& key, const uint8_t& value) {
  SetPersistedEntryProperty<std::string>(destination, key, std::string(1, static_cast<char>(value)));
}

template <>
uint64_t ExtractPersistedEntryProperty<uint64_t>(PersistedEntryProperties& source, const std::string& key) {
  return UnpackUint64BE(ExtractPersistedEntryProperty<std::string>(source, key));
}

template <>
void SetPersistedEntryProperty<uint64_t>(PersistedEntryProperties& destination, const std::string& key, const uint64_t& value) {
  SetPersistedEntryProperty<std::string>(destination, key, PackUint64BE(value));
}

template <>
EncryptionScheme ExtractPersistedEntryProperty<EncryptionScheme>(PersistedEntryProperties& source, const std::string& key) {
  return EncryptionScheme(ExtractPersistedEntryProperty<uint8_t>(source, key));
}

template <>
void SetPersistedEntryProperty<EncryptionScheme>(PersistedEntryProperties& destination, const std::string& key, const EncryptionScheme& value) {
  static_assert(EncryptionScheme::LATEST <= std::numeric_limits<uint8_t>::max()); // Ensure that we don't lose bits in our narrowing cast below
  assert(value >= 0);
  assert(value <= EncryptionScheme::LATEST);
  SetPersistedEntryProperty<uint8_t>(destination, key, static_cast<uint8_t>(value));
}

}
