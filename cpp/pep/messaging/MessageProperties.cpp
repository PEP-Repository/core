#include <pep/messaging/MessageProperties.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pep::messaging {

namespace {

// MessageProperties uses the (single) high bit to indicate message type
constexpr EncodedMessageProperties TypeRequest = 0x00000000;
constexpr EncodedMessageProperties TypeResponse = 0x80000000;
constexpr EncodedMessageProperties TypeBits = TypeRequest | TypeResponse;

// MessageProperties uses (the next-highest) three bits for state-related flags
constexpr EncodedMessageProperties FlagClose = 0x40000000; // This is the last piece of the (possibly multi-part) message
constexpr EncodedMessageProperties FlagError = 0x20000000; // The sending party encountered an error constructing or sending the (possibly multi-part) message. Implies FlagClose.
constexpr EncodedMessageProperties FlagPayload = 0x10000000; // The message includes content
constexpr EncodedMessageProperties FlagBits = FlagClose | FlagError | FlagPayload;

static_assert(FlagClose == Encode(Flags::Close));
static_assert(FlagError == Encode(Flags::Error));
static_assert(FlagPayload == Encode(Flags::Payload));
static_assert(FlagBits == Encode(Flags::All));

// MessageProperties uses remaining bits for a unique (serial) number for every request+response cycle
constexpr EncodedMessageProperties StreamIdBits = ~(TypeBits | FlagBits);

constexpr EncodedMessageProperties NoMessagePropertyBits = 0;

constexpr StreamId::Value ControlStreamId = 0;

}

bool MessageType::IsValidValue(Value value) noexcept {
  switch (value) {
  case Control:
  case Request:
  case Response:
    return true;
  }
  return false;
}

MessageType::MessageType(Value value)
  : value_(value) {
  assert(IsValidValue(value_));
}

std::string MessageType::describe() const {
  switch (value_) {
  case Request:
    return "request";
  case Response:
    return "response";
  case Control:
    return "control message";
  }
  throw std::runtime_error("Unsupported message type value " + std::to_string(value_));
}

EncodedMessageProperties MessageType::encode() const noexcept {
  assert(IsValidValue(value_));
  if (value_ == Response) {
    static_assert(TypeResponse != NoMessagePropertyBits);
    return TypeResponse;
  }
  return NoMessagePropertyBits;
}

void AssertValidCombination(Flags flags) {
  const auto ErrorMessage = [&](std::string_view reason) -> std::string{
    return (std::ostringstream{} << "Invalid Flag Combiation " << flags << " - " << reason).str();
  };

  if (HasFlags(flags, Flags::Error) && !HasFlags(flags, Flags::Close)) {
    throw std::invalid_argument(ErrorMessage("cannot have 'error' without 'close' flag"));
  }
  if (HasFlags(flags, Flags::Error | Flags::Payload)) {
    throw std::invalid_argument(ErrorMessage("cannot combine 'payload' with 'close' flag"));
  }
}

std::ostream& operator<<(std::ostream& out, Flags flags) {
  bool first = true;
  out << '{';
  auto printFlag = [&](std::string_view flag) {
    if (!std::exchange(first, false)) { out << ", "; }
    out << flag;
  };
  if (HasFlags(flags, Flags::Close)) { printFlag("close"); }
  if (HasFlags(flags, Flags::Error)) { printFlag("error"); }
  if (HasFlags(flags, Flags::Payload)) { printFlag("payload"); }
  out << '}';
  return out;
}

EncodedMessageProperties MessageId::encode() const noexcept {
  return type_.encode() | streamId_.encode();
}

bool StreamId::IsValidValue(Value value) noexcept {
  return (value & ~StreamIdBits) == NoMessagePropertyBits;
}

StreamId::StreamId(Value value)
  : value_(value) {
  assert(IsValidValue(value_));
}

StreamId StreamId::BeforeFirst() noexcept {
  return StreamId(0U);
}

StreamId StreamId::MakeNext(const StreamId& previous) noexcept {
  auto value = previous.value() + 1U;

  if (!IsValidValue(value)) { // ensure that our increment didn't spill into the (high) bits reserved for stuff other than the stream ID
    value = 1U;
  }
  if (value == ControlStreamId) { // ensure that we didn't wrap to zero (if ControlStreamId is ever changed)
    ++value;
  }
  assert(IsValidValue(value));

  return StreamId(value);
}

MessageId::MessageId(MessageType type, StreamId streamId)
  : type_(type), streamId_(streamId) {
}

MessageId MessageId::MakeForControlMessage() noexcept {
  return MessageId(MessageType::Control, StreamId(ControlStreamId));
}

MessageProperties::MessageProperties(MessageId messageId, Flags flags)
  : messageId_(messageId), flags_(flags) {
  AssertValidCombination(flags_);
  assert(flags_ == Flags::None || messageId_.type().value() != MessageType::Control);
}

EncodedMessageProperties MessageProperties::encode() const noexcept {
  return this->messageId().encode() | Encode(this->flags());
}

MessageProperties MessageProperties::DecodeFrom(EncodedMessageProperties properties) {
  auto typeBits = properties & TypeBits;
  auto flagBits = properties & FlagBits;
  auto streamId = properties & StreamIdBits;

  MessageType::Value type = MessageType::Request;
  if (streamId == ControlStreamId) {
    if (properties != ControlStreamId) {
      throw std::runtime_error("Message properties cannot specify a control stream ID with additional properties");
    }
    type = MessageType::Control;
  }
  else if ((typeBits & TypeResponse) != 0) {
    type = MessageType::Response;
  }

  Flags flags = static_cast<Flags>((flagBits & FlagBits) >> 28U);
  AssertValidCombination(flags);

  if (!StreamId::IsValidValue(streamId)) {
    throw std::runtime_error("Message properties specify an invalid stream ID");
  }

  return MessageProperties(MessageId(MessageType(type), StreamId(streamId)), flags);
}

MessageProperties MessageProperties::MakeForControlMessage() noexcept {
  return MessageProperties(MessageId::MakeForControlMessage(), Flags::None);
}

}
