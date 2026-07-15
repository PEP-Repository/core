#include <pep/messaging/MessageProperties.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <boost/algorithm/string/join.hpp>

namespace pep::messaging {

namespace {

using namespace detail;

static_assert(std::popcount(encoding_layout::TypeBits) == 1, "There is only one type bit, indicating 'Response'");
constexpr EncodedMessageProperties TypeResponseBit = encoding_layout::TypeBits;
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
  return (value_ == Response) ? TypeResponseBit : NoMessagePropertyBits;
}

void Flags::AssertValidCombination(Flags::Bits flags) {
  const auto ErrorMessage = [&](std::string_view reason) -> std::string{
    return (std::ostringstream{} << "Invalid Flag Combiation " << flags << " - " << reason).str();
  };

  if (HasFlags(flags, Flags::Bits::Error) && !HasFlags(flags, Flags::Bits::Close)) {
    throw std::invalid_argument(ErrorMessage("cannot have 'error' without 'close' flag"));
  }
  if (HasFlags(flags, Flags::Bits::Error | Flags::Bits::Payload)) {
    throw std::invalid_argument(ErrorMessage("cannot combine 'payload' with 'close' flag"));
  }
}

std::ostream& operator<<(std::ostream& out, Flags::Bits flags) {
  std::vector<std::string> flagNames;
  if (HasFlags(flags, Flags::Bits::Close)) { flagNames.push_back("close"); }
  if (HasFlags(flags, Flags::Bits::Error)) { flagNames.push_back("error"); }
  if (HasFlags(flags, Flags::Bits::Payload)) { flagNames.push_back("payload"); }
  return out << '{' << boost::algorithm::join(flagNames, ", ") << '}';
}

EncodedMessageProperties MessageId::encode() const noexcept {
  return type_.encode() | streamId_.encode();
}

bool StreamId::IsValidValue(Value value) noexcept {
  return (value & ~encoding_layout::StreamIdBits) == NoMessagePropertyBits;
}

StreamId::StreamId(Value value)
  : value_(value) {
  assert(IsValidValue(value_));
}

StreamId StreamId::BeforeFirst() noexcept {
  return StreamId(0U);
}

StreamId StreamId::MakeNext(const StreamId& previous) noexcept {
  static_assert(ControlStreamId == 0U, "We roll over to 1, so that we skip over the control stream id");
  constexpr auto maxStreamId = encoding_layout::StreamIdBits;
  return StreamId((previous.value() != maxStreamId) ? previous.value() + 1U : 1U);
}

MessageId::MessageId(MessageType type, StreamId streamId)
  : type_(type), streamId_(streamId) {
}

MessageId MessageId::MakeForControlMessage() noexcept {
  return MessageId(MessageType::Control, StreamId(ControlStreamId));
}

MessageProperties::MessageProperties(MessageId messageId, Flags flags)
  : messageId_(messageId), flags_(flags) {
  assert(flags_.bits() == Flags::Bits::None || messageId_.type().value() != MessageType::Control);
}

EncodedMessageProperties MessageProperties::encode() const noexcept {
  return this->messageId().encode() | this->flags().encode();
}

MessageProperties MessageProperties::DecodeFrom(EncodedMessageProperties properties) {
  const auto streamId = properties & encoding_layout::StreamIdBits;
  if (streamId == ControlStreamId && properties != ControlStreamId) {
    throw std::runtime_error("Message properties cannot specify a control stream ID with additional properties");
  }
  if (!StreamId::IsValidValue(streamId)) {
    throw std::runtime_error("Message properties specify an invalid stream ID");
  }

  const auto type =
      (streamId == ControlStreamId) ? MessageType::Control :
      (properties & TypeResponseBit) ? MessageType::Response :
      MessageType::Request;

  return MessageProperties(MessageId(MessageType(type), StreamId(streamId)), Flags::DecodeFrom(properties));
}

MessageProperties MessageProperties::MakeForControlMessage() noexcept {
  return MessageProperties(MessageId::MakeForControlMessage(), Flags{Flags::Bits::None});
}

}
