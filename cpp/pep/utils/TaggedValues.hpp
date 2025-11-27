#pragma once

#include <pep/utils/TypeTraits.hpp>
#include <any>
#include <cassert>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>

namespace pep {

/// @brief Container for heterogeneous value types identified by a tag type.
/// \code
///   // Common code
///   struct WorkingDirectoryTag { using type = std::filesystem::path; };
///   struct TrustedRootCAsTag   { using type = pep::X509RootCertificates; };
///
///   // Calling code
///   TaggedValues context;
///   context.set<WorkingDirectoryTag>(getcwd());
///   if (std::filesystem::exists(rootCaPath)) {
///     context.set<TrustedRootCAsTag>(pep::X509RootCertificates(pep::FromPem(ReadFile(rootCaPath))));
///   }
///
///   // Receiving code
///   if (auto rootCAs = context.get<TrustedRootCAsTag>()) {
///     myCertificateChain.validate(*rootCAs);
///   }
///   else {
///     throw std::runtime_error("Can't validate certificate chain");
///   }
/// \endcode
class TaggedValues {
private:
  std::unordered_map<std::type_index, std::any> mValues;

  template <typename TTag> static constexpr std::type_index KeyFor() noexcept { return std::type_index(typeid(TTag)); }
  template <typename TTag, typename TValues> static CopyConstness<typename TTag::type, TValues>* ConstAgnosticGet(TValues& values);

public:
  /// @brief Retrieves the value associated with the specified tag
  /// @tparam TTag The tag for which to acquire the value
  /// @return A pointer to the value for the specified tag, or NULL if the instance has no value for that tag
  template <typename TTag> typename TTag::type* get() noexcept { return ConstAgnosticGet<TTag>(mValues); }

  /// @brief Retrieves the value associated with the specified tag
  /// @tparam TTag The tag for which to acquire the value
  /// @return A pointer to the value for the specified tag, or NULL if the instance has no value for that tag
  template <typename TTag> const typename TTag::type* get() const noexcept { return ConstAgnosticGet<TTag>(mValues); }

  /// @brief Stores a value associated with the specified tag
  /// @tparam TTag The tag for which to store the value
  /// @param value The value to store
  /// @remark Overwrites any existing value
  template <typename TTag> void set(typename TTag::type value) { mValues.insert_or_assign(KeyFor<TTag>(), std::move(value)); }

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


/// @brief Implements lookup for TaggedValues::get (both const and non-const versions)
/// @tparam TTag The tag type for which to retrieve the value
/// @tparam TValues decltype(mValues) or the const-qualified equivalent
/// @param values The mValues member
/// @return The address of the value for the specified tag, or NULL if no value was found
/// @remark Less code than implementing the TaggedValues::get overloads individually
template <typename TTag, typename TValues>
CopyConstness<typename TTag::type, TValues>* TaggedValues::ConstAgnosticGet(TValues& values) {
  static_assert(std::is_same_v<decltype(TaggedValues::mValues), std::remove_const_t<TValues>>, "Parameter must be (possibly const qualified) TaggedValues::mValues");
  using Value = CopyConstness<typename TTag::type, TValues>;
  Value* result = nullptr;

  auto position = values.find(KeyFor<TTag>());
  if (position != values.end()) {
    auto& untyped = position->second;
    result = std::any_cast<Value>(&untyped);
    assert(result != nullptr);
  }

  return result;
}

}
