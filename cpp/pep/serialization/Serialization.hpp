#pragma once

#include <pep/serialization/Error.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <vector>
#include <list>

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

  template <typename T>
  static void CopyToRepeatedProtocolBuffer(::google::protobuf::RepeatedPtrField<typename Serializer<T>::ProtocolBufferType>& destination, std::vector<T> source) {
    Serializer<T>().copyToRepeatedProtocolBuffer(destination, std::move(source));
  }

  template <typename T>
  static void CopyToRepeatedProtocolBuffer(::google::protobuf::RepeatedPtrField<typename Serializer<T>::ProtocolBufferType>& destination, std::list<T> source) {
    Serializer<T>().copyToRepeatedProtocolBuffer(destination, std::move(source));
  }

  template <typename ProtoT>
  static typename ProtocolBufferedDeserialization<ProtoT>::DeserializedType FromProtocolBuffer(ProtoT&& source) {
    typedef typename ProtocolBufferedDeserialization<ProtoT>::DeserializedType T;
    return Serializer<T>().fromProtocolBuffer(std::move(source));
  }

  template <typename T>
  static void CopyFromRepeatedProtocolBuffer(std::vector<T>& destination, ::google::protobuf::RepeatedPtrField<typename Serializer<T>::ProtocolBufferType>&& source) {
    destination.reserve(static_cast<size_t>(source.size()));
    for (auto& x : source)
      destination.push_back(FromProtocolBuffer(std::move(x)));
  }

  template <typename T>
  static void CopyFromRepeatedProtocolBuffer(std::list<T>& destination, ::google::protobuf::RepeatedPtrField<typename Serializer<T>::ProtocolBufferType>&& source) {
    auto inserter = std::back_inserter(destination);
    auto end = source.end();
    for (auto i = source.begin(); i != end; ++i)
      *inserter = FromProtocolBuffer(std::move(*i));
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
