#include <pep/messaging/MessageHeader.hpp>
#include <pep/utils/BuildFlavor.hpp>
#include <pep/utils/Platform.hpp>
#include <format>
#include <stdexcept>

namespace pep::messaging {

const size_t MAX_SIZE_OF_MESSAGE =
// #1156: use larger message size in release builds so things will fail in debug builds (on dev boxes) before they bring prod down
#if BUILD_HAS_RELEASE_FLAVOR()
  2 *
#endif
  // TODO: reduce (back) to 1 Mb (i.e. remove multiplier by 2).
  // Value was increased as a temporary fix for (production) problems: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2238#note_30480
  2 * 1024 * 1024 - 4;


MessageHeader::MessageHeader(MessageLength length, MessageProperties properties)
  : mLength(length), mProperties(properties) {
  if (mProperties.messageId().type().value() == MessageType::CONTROL) {
    if (mLength != 0U) {
      throw std::runtime_error(std::format("Control messages must have zero length, length is {}", mLength));
    }
  }
}

MessageHeader::MessageHeader(MessageLength length, EncodedMessageProperties properties)
  : MessageHeader(length, MessageProperties::DecodeFrom(properties)) {
}

MessageHeader MessageHeader::MakeForControlMessage() noexcept {
  return MessageHeader(0U, MessageProperties::MakeForControlMessage());
}

EncodedMessageHeader MessageHeader::encode() const noexcept {
  return EncodedMessageHeader{
    .length = htonl(mLength),
    .properties = htonl(mProperties.encode())
  };
}

MessageHeader MessageHeader::Decode(const EncodedMessageHeader& encoded) {
  return MessageHeader(ntohl(encoded.length), ntohl(encoded.properties));
}

}
