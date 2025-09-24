#pragma once

#include <pep/serialization/Error.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <vector>

namespace pep {

// Convenience class for serialization routines
class Serialization {
public:
  // You should only use this function for tiny objects.
  template <typename T>
  static typename Serializer<T>::ProtocolBufferType
  ToProtocolBuffer(T value) {
    typename Serializer<T>::ProtocolBufferType ret;
    Serializer<T>().moveIntoProtocolBuffer(ret, std::move(value));
    return ret;
  }

  template <typename T>
  static void
  MoveIntoProtocolBuffer(typename Serializer<T>::ProtocolBufferType& dest, T value) {
    Serializer<T>().moveIntoProtocolBuffer(dest, std::move(value));
  }

  template <std::ranges::input_range SourceRange>
  static void AssignToRepeatedProtocolBuffer(
      ::google::protobuf::RepeatedPtrField<typename Serializer<std::ranges::range_value_t<SourceRange>>::ProtocolBufferType>& destination,
      SourceRange source) {
    using T = std::ranges::range_value_t<SourceRange>;
    Serializer<T>().assignToRepeatedProtocolBuffer(destination, std::move(source));
  }

  template <typename ProtoT>
  static typename ProtocolBufferedDeserialization<ProtoT>::DeserializedType FromProtocolBuffer(ProtoT&& source) {
    static_assert(std::is_rvalue_reference_v<decltype(source)>, "You should use std::move with FromProtocolBuffer");
    using T = typename ProtocolBufferedDeserialization<ProtoT>::DeserializedType;
    return Serializer<T>().fromProtocolBuffer(std::forward<ProtoT>(source)); // Effectively an std::move, but the linter complains if we invoke that directly
  }

  template <std::ranges::range ResultCollection>
  static void AssignFromRepeatedProtocolBuffer(
      ResultCollection& destination,
      ::google::protobuf::RepeatedPtrField<typename Serializer<std::ranges::range_value_t<ResultCollection>>::ProtocolBufferType>&& source) {
    using T = std::ranges::range_value_t<ResultCollection>;
    using ProtoT = typename Serializer<T>::ProtocolBufferType;
    destination = RangeToCollection<ResultCollection>(source | std::views::transform([](ProtoT& sourceElem) {
      return FromProtocolBuffer(std::move(sourceElem));
    }));
  }

  template <typename T>
  static void SerializeToOstream(std::ostream& destination, T value) {
    Serializer<T>().serializeToOstream(destination, std::move(value));
  }

  template <typename T>
  static std::string ToString(T value, bool withMagic = true) {
    return Serializer<T>().toString(std::move(value), withMagic);
  }

  template <typename T>
  static std::vector<char> ToCharVector(T value, bool withMagic = true) {
    auto s = Serializer<T>().toString(std::move(value), withMagic);
    return std::vector<char>(s.begin(), s.end());
  }

  template <typename T>
  static T FromCharVector(const std::vector<char>& source, bool withMagic = true) {
    return Serializer<T>().fromString(
      std::string(source.begin(), source.end()),
      withMagic
    );
  }

  template <typename T>
  static T FromString(std::string source, bool withMagic = true) {
    return Serializer<T>().fromString(std::move(source), withMagic);
  }

  template <typename T>
  static T FromStringOrRaiseError(std::string source) {
    static_assert(!std::is_same_v<T, Error>, "Ambiguous: should Error instance be raised or deserialized?");
    Error::ThrowIfDeserializable(source);
    return FromString<T>(source);
  }

  template <typename T>
  static T FromJsonString(const std::string &message) {
    return Serializer<T>().fromJsonString(message);
  }

  template <typename T>
  static std::string ToJsonString(const T& value) {
    return Serializer<T>().toJsonString(value);
  }

  template <typename T>
  static T ParseEnum(const std::string& name) {
    return Serializer<T>().parse(name);
  }

  template <typename T>
  static std::string ToEnumString(const T& value) {
    return Serializer<T>().toEnumString(value);
  }
};

}
