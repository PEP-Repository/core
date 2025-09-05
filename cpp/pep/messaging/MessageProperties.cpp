#include <pep/messaging/MessageProperties.hpp>

#include <boost/format.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pep::messaging {

namespace {

// MessageProperties uses the (single) high bit to indicate message type
constexpr EncodedMessageProperties TYPE_REQUEST = 0x00000000;
constexpr EncodedMessageProperties TYPE_RESPONSE = 0x80000000;
constexpr EncodedMessageProperties TYPE_BITS = TYPE_REQUEST | TYPE_RESPONSE;

// MessageProperties uses (the next-highest) three bits for state-related flags
constexpr EncodedMessageProperties FLAG_CLOSE = 0x40000000; // This is the last piece of the (possibly multi-part) message
constexpr EncodedMessageProperties FLAG_ERROR = 0x20000000; // The sending party encountered an error constructing or sending the (possibly multi-part) message. Implies FLAG_CLOSE.
constexpr EncodedMessageProperties FLAG_PAYLOAD = 0x10000000; // The message includes content
constexpr EncodedMessageProperties FLAG_BITS = FLAG_CLOSE | FLAG_ERROR | FLAG_PAYLOAD;

// MessageProperties uses remaining bits for a unique (serial) number for every request+response cycle
constexpr EncodedMessageProperties STREAM_ID_BITS = ~(TYPE_BITS | FLAG_BITS);

constexpr EncodedMessageProperties NO_MESSAGE_PROPERTY_BITS = 0;

constexpr StreamId::Value CONTROL_STREAM_ID = 0;

}

bool MessageType::IsValidValue(Value value) noexcept {
  switch (value) {
  case CONTROL:
  case REQUEST:
  case RESPONSE:
    return true;
  }
  return false;
}

MessageType::MessageType(Value value)
  : mValue(value) {
  assert(IsValidValue(mValue));
}

std::string MessageType::describe() const {
  switch (mValue) {
  case REQUEST:
    return "request";
  case RESPONSE:
    return "response";
  case CONTROL:
    return "control message";
  }
  throw std::runtime_error("Unsupported message type value " + std::to_string(mValue));
}

EncodedMessageProperties MessageType::encode() const noexcept {
  assert(IsValidValue(mValue));
  if (mValue == RESPONSE) {
    static_assert(TYPE_RESPONSE != NO_MESSAGE_PROPERTY_BITS);
    return TYPE_RESPONSE;
  }
  return NO_MESSAGE_PROPERTY_BITS;
}

EncodedMessageProperties Flags::encode() const noexcept {
  EncodedMessageProperties result = NO_MESSAGE_PROPERTY_BITS;

  if (mClose) {
    result |= FLAG_CLOSE;
  }
  if (mError) {
    result |= FLAG_ERROR;
  }
  if (mPayload) {
    result |= FLAG_PAYLOAD;
  }

  return result;
}

Flags Flags::MakeEmpty() noexcept {
  return Flags(false, false, false);
}

Flags Flags::MakeError() noexcept {
  return Flags(true, true, false);
}

Flags Flags::MakePayload(bool close) noexcept {
  return Flags(close, false, true);
}

Flags Flags::MakeClose(bool payload) noexcept {
  return Flags(true, false, payload);
}

bool Flags::areValid() const noexcept {
  if (mError) {
    if (mPayload) { // Error messages cannot have payload (and vice versa)
      return false;
    }
    if (!mClose) { // Error implies close (and that bit must be set)
      return false;
    }
  }
  return true;
}

bool Flags::empty() const noexcept {
  return !mClose && !mError && !mPayload;
}

Flags::Flags(bool close, bool error, bool payload)
  : mClose(close), mError(error), mPayload(payload) {
  if (!areValid()) {
    throw std::invalid_argument((boost::format("These flags cannot be combined: %s") % *this).str());
  }
}

Flags Flags::operator|(const Flags& other) const {
  return Flags(this->close() || other.close(), this->error() || other.error(), this->payload() || other.payload());
}

std::ostream& operator<<(std::ostream& out, Flags flags) {
  bool first = true;
  auto printFlag = [&](std::string_view flag) {
    if (!std::exchange(first, false)) { out << ", "; }
    out << flag;
  };
  if (flags.close()) { printFlag("close"); }
  if (flags.error()) { printFlag("error"); }
  if (flags.payload()) { printFlag("payload"); }
  return out;
}

EncodedMessageProperties MessageId::encode() const noexcept {
  return mType.encode() | mStreamId.encode();
}

bool StreamId::IsValidValue(Value value) noexcept {
  return (value & ~STREAM_ID_BITS) == NO_MESSAGE_PROPERTY_BITS;
}

StreamId::StreamId(Value value)
  : mValue(value) {
  assert(IsValidValue(mValue));
}

StreamId StreamId::BeforeFirst() noexcept {
  return StreamId(0U);
}

StreamId StreamId::MakeNext(const StreamId& previous) noexcept {
  auto value = previous.value() + 1U;

  if (!IsValidValue(value)) { // ensure that our increment didn't spill into the (high) bits reserved for stuff other than the stream ID
    value = 1U;
  }
  if (value == CONTROL_STREAM_ID) { // ensure that we didn't wrap to zero (if CONTROL_STREAM_ID is ever changed)
    ++value;
  }
  assert(IsValidValue(value));

  return StreamId(value);
}

MessageId::MessageId(MessageType type, StreamId streamId)
  : mType(type), mStreamId(streamId) {
}

MessageId MessageId::MakeForControlMessage() noexcept {
  return MessageId(MessageType::CONTROL, StreamId(CONTROL_STREAM_ID));
}

MessageProperties::MessageProperties(MessageId messageId, Flags flags)
  : mMessageId(messageId), mFlags(flags) {
  assert(mFlags.empty() || mMessageId.type().value() != MessageType::CONTROL);
}

EncodedMessageProperties MessageProperties::encode() const noexcept {
  return this->messageId().encode() | this->flags().encode();
}

MessageProperties MessageProperties::DecodeFrom(EncodedMessageProperties properties) {
  auto typeBits = properties & TYPE_BITS;
  auto flagBits = properties & FLAG_BITS;
  auto streamId = properties & STREAM_ID_BITS;

  MessageType::Value type;
  if (streamId == CONTROL_STREAM_ID) {
    if (properties != CONTROL_STREAM_ID) {
      throw std::runtime_error("Message properties cannot specify a control stream ID with additional properties");
    }
    type = MessageType::CONTROL;
  }
  else if (typeBits & TYPE_RESPONSE) {
    type = MessageType::RESPONSE;
  }
  else {
    type = MessageType::REQUEST;
  }

  Flags flags(flagBits & FLAG_CLOSE, flagBits & FLAG_ERROR, flagBits & FLAG_PAYLOAD);

  if (!StreamId::IsValidValue(streamId)) {
    throw std::runtime_error("Message properties specify an invalid stream ID");
  }

  return MessageProperties(MessageId(MessageType(type), StreamId(streamId)), flags);
}

MessageProperties MessageProperties::MakeForControlMessage() noexcept {
  return MessageProperties(MessageId::MakeForControlMessage(), Flags::MakeEmpty());
}

}
