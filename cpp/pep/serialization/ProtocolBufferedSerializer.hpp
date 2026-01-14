#pragma once

#include <pep/serialization/MessageSerializer.hpp>
#include <pep/serialization/Serializer.hpp>

#include <limits>
#include <ranges>

#ifdef __clang__ // Prevent "unknown pragma" from other compilers
// Work around Clang deprecation warning for protobuf header, despite it being marked as a system header.
// See https://github.com/llvm/llvm-project/issues/76515
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif // __clang__

#include <google/protobuf/repeated_field.h>
#include <google/protobuf/util/json_util.h>

#ifdef __clang__ // Prevent "unknown pragma" from other compilers
#pragma clang diagnostic pop
#endif // __clang__

#define PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(t, protot) \
  template <> \
  struct ProtocolBufferedSerialization<t> { \
    using ProtocolBufferType = protot; \
  }; \
  \
  template <> \
  struct ProtocolBufferedDeserialization<protot> { \
    using DeserializedType = t; \
  }

#define PEP_DEFINE_ENUM_SERIALIZER(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(t, proto::t); \
  \
  template <> \
  class Serializer<t> : public ProtocolBufferedEnumSerializer<t> { \
  public: \
    Serializer() : ProtocolBufferedEnumSerializer<t>(proto::BOOST_PP_CAT(t, _descriptor)) {} \
  }

#define PEP_DEFINE_CODED_SERIALIZER(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(t, proto::t); \
  \
  template <> \
  class Serializer<t> : public ProtocolBufferedMessageSerializer<t> { \
  public: \
    void moveIntoProtocolBuffer(proto::t& dest, t value) const override; \
    t fromProtocolBuffer(proto::t&& source) const override; \
  }

#define PEP_DEFINE_SHARED_PTR_SERIALIZER(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(std::shared_ptr<t>, proto::t); \
  \
  template <> \
  /*NOLINTNEXTLINE(bugprone-macro-parentheses) `t` is a type*/ \
  class Serializer<std::shared_ptr<t>> : public ProtocolBufferedMessageSerializer<std::shared_ptr<t>> { \
  public: \
    void moveIntoProtocolBuffer(proto::t& dest, std::shared_ptr<t> value) const override; \
    std::shared_ptr<t> fromProtocolBuffer(proto::t&& source) const override; \
  }

#define PEP_DEFINE_EMPTY_SERIALIZER(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(t, proto::t); \
  \
  template <> \
  class Serializer<t> : public ProtocolBufferedEmptyMessageSerializer<t> {}

namespace pep {

/* Specialize these two structs to define which PEP type corresponds with which
  * protocol buffers type and vice versa. The structs must be specialized consistently,
  * i.e. if ProtocolBufferedSerialization<Foo>::ProtocolBufferType is proto::Bar,
  * then ProtocolBufferedDeserialization<proto::Bar>::DeserializedType must be Foo.
  */
template <typename T>
struct ProtocolBufferedSerialization; // Specialize with a nested ProtocolBufferType typedef
template <typename ProtoT>
struct ProtocolBufferedDeserialization; // Specialize with a nested DeserializedType typedef

// Inheritance requires properly specialized ProtocolBufferedSerialization<> and ProtocolBufferedDeserialization<> structs
template <typename T>
class ProtocolBufferedSerializer {
public:
  using ProtocolBufferType = typename ProtocolBufferedSerialization<T>::ProtocolBufferType;
  static_assert(std::is_same<T, typename ProtocolBufferedDeserialization<ProtocolBufferType>::DeserializedType>::value,
    "Serialization and deserialization types must match");

  ProtocolBufferedSerializer() = default;
  virtual ~ProtocolBufferedSerializer() = default;

  ProtocolBufferedSerializer(const ProtocolBufferedSerializer&) = default;
  ProtocolBufferedSerializer& operator=(const ProtocolBufferedSerializer&) = default;

  virtual void moveIntoProtocolBuffer(ProtocolBufferType& dest, T value) const = 0;
  virtual T fromProtocolBuffer(ProtocolBufferType&& source) const = 0;

  void assignToRepeatedProtocolBuffer(::google::protobuf::RepeatedPtrField<ProtocolBufferType>& destination, std::ranges::sized_range auto source)
  requires(std::same_as<T, std::ranges::range_value_t<decltype(source)>>) {
    size_t size{std::ranges::size(source)};
    // Protobuf doesn't check if elements actually fit
    if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
      throw std::runtime_error("Too many container elements to copy to protocol buffer");
    }

    destination.Clear();
    destination.Reserve(static_cast<int>(size));
    for (auto& elem : source) {
      moveIntoProtocolBuffer(*destination.Add(), std::move(elem));
    }
  }
};

// Inheritance requires that enumerators and their values match between T and the corresponding protobuf type
template <typename T>
class ProtocolBufferedEnumSerializer : public ProtocolBufferedSerializer<T> {
public:
  using typename ProtocolBufferedSerializer<T>::ProtocolBufferType;

  static_assert(std::is_enum<T>::value, "T must be an enum type");
  static_assert(std::is_enum<ProtocolBufferType>::value, "Protocol buffer type must be an enum");

private:
  const ::google::protobuf::EnumDescriptor* mDescriptor;

protected:
  ProtocolBufferedEnumSerializer(std::function<const ::google::protobuf::EnumDescriptor*()> getDescriptor)
    : mDescriptor(getDescriptor()) {
    if (mDescriptor == nullptr) {
      throw std::runtime_error("Serializer could not find EnumDescriptor");
    }
  }

public:
  void moveIntoProtocolBuffer(ProtocolBufferType& dest, T value) const override {
    dest = static_cast<ProtocolBufferType>(static_cast<int>(value));
  }

  T fromProtocolBuffer(ProtocolBufferType&& source) const override {
    return static_cast<T>(static_cast<int>(source));
  }

  T parse(const std::string& name) const {
    auto valueDescriptor = mDescriptor->FindValueByName(name);
    if (valueDescriptor == nullptr) {
      throw std::runtime_error("Unknown enumerator name " + name);
    }
    return T(valueDescriptor->number());
  }

  std::string_view toEnumString(T value) const {
    auto valueDescriptor = mDescriptor->FindValueByNumber(static_cast<int>(value));
    if (valueDescriptor == nullptr) {
      throw std::runtime_error("Unknown enumerator value " + std::to_string(value));
    }
    return std::string(valueDescriptor->name());
  }
};

template <typename T>
class ProtocolBufferedMessageSerializer : public MessageSerializer<T>, public ProtocolBufferedSerializer<T> {
public:
  using typename ProtocolBufferedSerializer<T>::ProtocolBufferType;
  static_assert(std::is_base_of<google::protobuf::Message, ProtocolBufferType>::value, "Protocol buffer type must inherit from Message");

public:
  T fromJsonString(const std::string &message) {
    ProtocolBufferType buffer;
    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;
    auto status = google::protobuf::util::JsonStringToMessage(message, &buffer, options);
    if (!status.ok()) {
      throw SerializeException("Failed to deserialize from JSON: " + status.ToString());
    }
    return this->fromProtocolBuffer(std::move(buffer));
  }

  std::string toJsonString(const T& value) {
    ProtocolBufferType buffer;
    this->moveIntoProtocolBuffer(buffer, value);
    std::string result;
    auto status = google::protobuf::util::MessageToJsonString(buffer, &result);
    if (!status.ok()) {
      throw SerializeException("Failed to serialize to JSON: " + status.ToString());
    }
    return result;
  }

  void serializeToOstream(std::ostream& destination, T value) const override {
    ProtocolBufferType buffer;
    this->moveIntoProtocolBuffer(buffer, std::move(value));
    if (!buffer.SerializeToOstream(&destination)) {
      throw SerializeException("Object could not be serialized to stream");
    }
  }

  T parseFromIstream(std::istream& source) const override {
    ProtocolBufferType buffer;
    if (!buffer.ParseFromIstream(&source)) {
      throw SerializeException("Object could not be deserialized from stream");
    }
    return this->fromProtocolBuffer(std::move(buffer));
  }

  std::string toString(ProtocolBufferType buffer, bool withMagic = true) const {
    std::string ret;
    if (withMagic) {
      std::ostringstream magicStream;
      MessageMagician<T>::WriteMagicTo(magicStream);
      ret = std::move(magicStream).str();
    }
    buffer.AppendToString(&ret);
    return ret;
  }

  std::string toString(T value, bool withMagic = true) const override {
    ProtocolBufferType buffer;
    this->moveIntoProtocolBuffer(buffer, std::move(value));
    return this->toString(std::move(buffer), withMagic);
  }

  T fromString(std::string_view szMessage, bool withMagic = true) const override {
    if (withMagic) {
      szMessage = MessageMagician<T>::SkipMessageMagic(szMessage);
    }
    const char* msg = szMessage.data();
    size_t msg_size = szMessage.size();
    ProtocolBufferType buffer;
    if (msg_size > static_cast<unsigned int>(INT_MAX)) {
      throw SerializeException("Message too long to deserialize from string");
    }
    if (!buffer.ParseFromArray(msg, static_cast<int>(msg_size)))
      throw SerializeException("Object could not be deserialized from string");
    return this->fromProtocolBuffer(std::move(buffer));
  }
};

template <typename T>
class ProtocolBufferedEmptyMessageSerializer : public ProtocolBufferedMessageSerializer<T> {
protected:
  ProtocolBufferedEmptyMessageSerializer() noexcept = default;

public:
  using typename ProtocolBufferedMessageSerializer<T>::ProtocolBufferType;

  void moveIntoProtocolBuffer(ProtocolBufferType& dest, T value) const override { }
  T fromProtocolBuffer(ProtocolBufferType&& source) const override {
    return T();
  }
};

}
