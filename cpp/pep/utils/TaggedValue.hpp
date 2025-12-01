#pragma once

#include <pep/utils/TypeTraits.hpp>
#include <any>
#include <cassert>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include <boost/exception/error_info.hpp>

namespace pep {

/// @brief Associates (a value of) a particular type with a (unique) tag type, allowing such values to be stored a TaggedValues container
/// @tparam TValue The type of value associated with the tag
/// @tparam TTag The (unique) tag type associated with the value. Usually a named struct introduced specifically to identify a TaggedValue
/// \code
///   using TaggedWorkingDirectory = TaggedValue<std::filesystem::path, struct WorkingDirectoryTag>;
///   using TaggedTempDirectory    = TaggedValue<std::filesystem::path, struct TempDirectoryTag>;
///   using TaggedRootCAs          = TaggedValue<X509RootCertificates,  struct RootCAsTag>;
/// \endcode
/// @remark Based on Boost's error_info, but note the reversed template parameter order. See https://www.boost.org/doc/libs/latest/libs/exception/doc/error_info.html
template <typename TValue, typename TTag>
class TaggedValue {
public:
  using value_type = TValue;
  using tag_type = TTag;

private:
  value_type mValue;

public:
  explicit TaggedValue(value_type value) noexcept(noexcept(value_type(std::move(value)))) : mValue(std::move(value)) {}

  value_type& value() noexcept { return mValue; }
  const value_type& value() const noexcept { return mValue; }

  auto operator<=>(const TaggedValue&) const = default;
};

/// @brief Container for TaggedValue instances. Can store a single value of each TaggedValue specialization.
/// \code
///   // Common code
///   using TaggedWorkingDirectory = TaggedValue<std::filesystem::path, struct WorkingDirectoryTag>;
///   using TaggedTempDirectory    = TaggedValue<std::filesystem::path, struct TempDirectoryTag>;
///   using TaggedRootCAs          = TaggedValue<X509RootCertificates,  struct RootCAsTag>;
///
///   // Receiving code
///   X509CertificateChain Validated(const X509CertificateChain& chain, const X509RootCertificates& rootCAs); // Defined elsewhere
/// 
///   X509CertificateChain Validated(const X509CertificateChain& chain, const TaggedValues& context) {
///     if (auto rootCAs = context.get_value<TaggedRootCAs>()) {  // If the container has the TaggedValue that we need...
///       return Validated(chain, *rootCAs);                      // ... process it
///     }
///     throw std::runtime_error("No root CAs available to validate certificate chain");
///   }
///
///   // Calling code
///   TaggedValues context;
///   context.set(TaggedWorkingDirectory(std::filesystem::current_path()));
///   if (std::filesystem::exists(rootCaPath)) {
///     context.set(TaggedRootCAs(X509RootCertificates(FromPem(ReadFile(rootCaPath)))));
///   }
///   // ...
///   auto chain = Validated(X509CertificateChain(FromPem(ReadFile(chainPath))), context);
/// \endcode
class TaggedValues {
private:
  std::unordered_map<std::type_index, std::any> mValues;

  template <typename TTagged> static std::type_index KeyFor() noexcept;
  template <typename TTagged, typename TValues> static auto ConstAgnosticGet(TValues& values);
  template <typename TTagged, typename TValues> static auto ConstAgnosticGetValue(TValues& values);

public:
  /// @brief Retrieves the TaggedValue associated with the specified tag
  /// @tparam TTagged The TaggedValue specialization to retrieve
  /// @return A pointer to the stored TaggedValue, or NULL if the instance contains no value of that type
  template <typename TTagged> auto get() noexcept { return ConstAgnosticGet<TTagged>(mValues); }

  /// @brief Retrieves the TaggedValue associated with the specified tag
  /// @tparam TTagged The TaggedValue specialization to retrieve
  /// @return A pointer to the stored TaggedValue, or NULL if the instance contains no value of that type
  template <typename TTagged> auto get() const noexcept { return ConstAgnosticGet<TTagged>(mValues); }

  /// @brief Retrieves the _payload) value associated with the specified tag
  /// @tparam TTagged The TaggedValue specialization whose value to retrieve
  /// @return A pointer to the stored TaggedValue::value_type, or NULL if the instance does not contain the specified TaggedValue
  template <typename TTagged> auto get_value() noexcept { return ConstAgnosticGetValue<TTagged>(mValues); }

  /// @brief Retrieves the _payload) value associated with the specified tag
  /// @tparam TTagged The TaggedValue specialization whose value to retrieve
  /// @return A pointer to the stored TaggedValue::value_type, or NULL if the instance does not contain the specified TaggedValue
  template <typename TTagged> auto get_value() const noexcept { return ConstAgnosticGetValue<TTagged>(mValues); }

  /// @brief Stores a TaggedValue
  /// @tparam TTagged The TaggedValue specialization to store
  /// @param value The TaggedValue to store
  /// @return TRUE if the value was newly inserted; FALSE if it was overwritten.
  /// @remark Overwrites any existing value
  template <typename TTagged> bool set(TTagged value) { return mValues.insert_or_assign(KeyFor<TTagged>(), std::move(value)).second; }

  /// @brief Stores a new TaggedValue
  /// @tparam TTagged The TaggedValue specialization to store
  /// @param value The TaggedValue to store
  /// @remark Throws an exception if the instance already stores the specified TaggedValue
  template <typename TTagged> void add(TTagged value);

  /// @brief Discards the value associated with the specified tag
  /// @tparam TTag The tag for which to discard the value
  /// @remark A no-op if no value has been stored for the specified tag
  template <typename TTag> void unset() { mValues.erase(KeyFor<TTag>()); }

  /// @brief Discards all values
  void clear() { mValues.clear(); }

  /// @brief Determines if the instance is empty
  /// @return TRUE if the instance stores no values; FALSE if it stores any
  size_t empty() const noexcept { return mValues.empty(); }

  /// @brief Determines the number of values stored in the instance
  /// @return The number of stored values
  size_t size() const noexcept { return mValues.size(); }
};

template <typename TTagged> void TaggedValues::add(TTagged value) {
  if (this->get<TTagged>()) {
    throw std::runtime_error("The specified TaggedValue already exists");
  }
  this->set(std::move(value));
}

/// @brief Implements lookup for TaggedValues::get (both const and non-const versions)
/// @tparam TTagged The TaggedValue specialization for which to retrieve the value
/// @tparam TValues decltype(mValues) or the const-qualified equivalent
/// @param values The mValues member
/// @return The address of the specified TaggedValue, or NULL if the instance does not contain the specified TaggedValue
/// @remark Less code than implementing the TaggedValues::get overloads individually
template <typename TTagged, typename TValues>
auto TaggedValues::ConstAgnosticGet(TValues& values) {
  static_assert(std::is_same_v<decltype(TaggedValues::mValues), std::remove_const_t<TValues>>, "Parameter must be (possibly const qualified) TaggedValues::mValues");

  using Value = CopyConstness<TTagged, TValues>;
  Value* result = nullptr;

  auto position = values.find(KeyFor<TTagged>());
  if (position != values.end()) {
    auto& untyped = position->second;
    result = std::any_cast<Value>(&untyped);
    assert(result != nullptr);
  }

  return result;
}

/// @brief Convenience method to retrieve the (payload) value for a TaggedValue
/// @tparam TTagged The TaggedValue specialization for which to retrieve the value
/// @tparam TValues decltype(mValues) or the const-qualified equivalent
/// @param values The mValues member
/// @return The address of the (payload, i.e. value_type) value of the specified TaggedValue, or NULL if the instance does not contain the specified TaggedValue
template <typename TTagged, typename TValues>
auto TaggedValues::ConstAgnosticGetValue(TValues& values) {
  auto tagged = ConstAgnosticGet<TTagged>(values); // (Also) enforces that TTagged is a supported type, producing a compiler error if not

  using Value = CopyConstness<typename TTagged::value_type, TValues>;
  Value* result = nullptr;
  if (tagged != nullptr) {
    result = &tagged->value();
  }

  return result;
}

/// @brief Returns the (unordered_map) key for the specified TaggedValue specialization
/// @tparam TTagged The TaggedValue specialization (type) to produce the key for
/// @return A value usable as a key in std containers.
/// @remark Asserts at compile time that TTagged is a specialization of TaggedValue
template <typename TTagged>
std::type_index TaggedValues::KeyFor() noexcept {
  static_assert(std::is_same_v<TTagged, TaggedValue<typename TTagged::value_type, typename TTagged::tag_type>>, "TTagged must be a TaggedValue<> specialization");
  return std::type_index(typeid(TTagged));
}

}
