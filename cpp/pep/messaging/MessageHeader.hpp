#pragma once

#include <pep/messaging/MessageProperties.hpp>

namespace pep::messaging {

using MessageLength = uint32_t;

extern const size_t MAX_SIZE_OF_MESSAGE;

// Helper struct to send and receive message headers across the network
#pragma pack(push, 1)
struct EncodedMessageHeader {
  MessageLength length; // in network order, i.e. big-endian)
  EncodedMessageProperties properties; // in network order, i.e. big-endian
};
#pragma pack(pop)

// boost::asio::async_read is going to memcpy into the EncodedMessageHeader struct
static_assert(sizeof(EncodedMessageHeader) == sizeof(EncodedMessageHeader::length) + sizeof(EncodedMessageHeader::properties));


class MessageHeader {
public:

  MessageHeader(MessageLength length, MessageProperties properties);
  MessageHeader(MessageLength length, EncodedMessageProperties properties); // parameters in host (hardware) order, e.g. little endian on x86_64

  static MessageHeader MakeForControlMessage() noexcept;

  MessageLength length() const noexcept { return mLength; }
  const MessageProperties& properties() const noexcept { return mProperties; }

  EncodedMessageHeader encode() const noexcept;
  static MessageHeader Decode(const EncodedMessageHeader& encoded);

private:
  MessageLength mLength;
  MessageProperties mProperties;
};

}
