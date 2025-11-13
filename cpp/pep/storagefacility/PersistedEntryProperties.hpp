#pragma once

#include <pep/morphing/Metadata.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

/* Legacy: file store entries used to store most of their properties in a string-to-string map, which was
 * then serialized to disk. While the file store entry class was refactored to reduce memory use, disk
 * serialization was kept backward compatible. Entries are therefore (now) stored in a two-step process:
 * - the entry writes its properties to a string-to-string map, and
 * - that map is then serialized to disk just like it was before.
 * This source provides the "PersistedEntryProperties" typedef for the string-to-string map, plus some functions
 * to access the map's data.
 *
 * Note that custom metadata entries (MetadataXEntry instances) are stored with the key prefixed with
 * "x-", to allow custom entries (such as "x-filesize") to be distinguished from non-custom ones (such as
 * "filesize"). The functions in this source only deal with value storage and assume that the caller
 * performs the necessary prefixing. We do so in the EntryContent::Load and EntryContent::Save methods.
 */

 /*!
  * \brief The structure used to store file store entry properties when they are persisted.
  */
using PersistedEntryProperties = std::map<std::string, std::string>;

/*!
 * \brief Reads a property and removes it from the structure.
 * \tparam T The type of property (value) to extract.
 * \param source The structure containing the property.
 * \param key The key (name) under which the property is stored.
 * \return The property's value.
 * \remark This template function implements the general case: values are stored by serializing the value (type).
 *         Specializations exist for certain (e.g. non-serializable) value types.
 * \remark Removes the (named) property so that remaining (x-prefixed) entries can be processed generically/iteratively.
 */
template <typename T>
T ExtractPersistedEntryProperty(PersistedEntryProperties& source, const std::string& key) {
  return Serialization::FromString<T>(ExtractPersistedEntryProperty<std::string>(source, key));
}

/*!
 * \brief Sets a property to a certain value in the structure.
 * \tparam T The type of property (value) to assign.
 * \param destination The structure that should hold the property.
 * \param key The key (name) under which the property is stored.
 * \param value The value to store for the property.
 * \remark This template function implements the general case: values are stored by serializing the value (type).
 *         Specializations exist for certain (e.g. non-serializable) value types.
 */
template <typename T>
void SetPersistedEntryProperty(PersistedEntryProperties& destination, const std::string& key, const T& value) {
  SetPersistedEntryProperty<std::string>(destination, key, Serialization::ToString(value));
}

//! Specialized property extraction for string values.
template <> std::string ExtractPersistedEntryProperty<std::string>(PersistedEntryProperties& source, const std::string& key);
//! Specialized property assignment for string values.
template <> void SetPersistedEntryProperty<std::string>(PersistedEntryProperties& destination, const std::string& key, const std::string& value);

//! Specialized property extraction for uint8_t values.
template <> uint8_t ExtractPersistedEntryProperty<uint8_t>(PersistedEntryProperties& source, const std::string& key);
//! Specialized property assignment for uint8_t values.
template <> void SetPersistedEntryProperty<uint8_t>(PersistedEntryProperties& destination, const std::string& key, const uint8_t& value);

//! Specialized property extraction for uint64_t values.
template <> uint64_t ExtractPersistedEntryProperty<uint64_t>(PersistedEntryProperties& source, const std::string& key);
//! Specialized property assignment for uint64_t values.
template <> void SetPersistedEntryProperty<uint64_t>(PersistedEntryProperties& destination, const std::string& key, const uint64_t& value);

template <> Timestamp ExtractPersistedEntryProperty<Timestamp>(PersistedEntryProperties& source, const std::string& key);
template <> void SetPersistedEntryProperty<Timestamp>(PersistedEntryProperties& destination, const std::string& key, const Timestamp& value);

//! Specialized property extraction for EncryptionScheme (enumerator) values.
template <> EncryptionScheme ExtractPersistedEntryProperty<EncryptionScheme>(PersistedEntryProperties& source, const std::string& key);
//! Specialized property assignment for EncryptionScheme (enumerator) values.
template <> void SetPersistedEntryProperty<EncryptionScheme>(PersistedEntryProperties& destination, const std::string& key, const EncryptionScheme& value);

/*!
 * \brief Reads a property from the structure, returning it and removing it from the structure if it exists.
 * \tparam T The type of property to extract.
 * \param source The structure containing the property.
 * \param key The key (name) under which the property is stored.
 * \return The property's value, or std::nullopt if the property was not present in the structure.
 */
template <typename T>
std::optional<T> TryExtractPersistedEntryProperty(PersistedEntryProperties& source, const std::string& key) {
  if (source.find(key) != source.end()) {
    return ExtractPersistedEntryProperty<T>(source, key);
  }
  return std::nullopt;
}

}
