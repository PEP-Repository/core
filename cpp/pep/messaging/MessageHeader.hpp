#pragma once

#include <pep/messaging/MessageProperties.hpp>

namespace pep::messaging {

using MessageLength = uint32_t;

extern const size_t MaxSizeOfMessage;

/// @brief Aggressive guesstimate of the net message capacity for serialization
/// @remark Fraction of capacity available for unserialized payload/business objects.
///         E.g. if this value is 0.9, unserialized object size should not exceed 90% of capacity available for serialization.
extern const double NetMessageCapacityFactor;

/// @brief Guesstimate of the maximum size of a business object so its serialized form does not exceed MaxSizeOfMessage.
/// @remark Intended for use with the "FillToCapacity" function.
extern const size_t NetMessageCapacity;

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

  MessageLength length() const noexcept { return length_; }
  const MessageProperties& properties() const noexcept { return properties_; }

  EncodedMessageHeader encode() const noexcept;
  static MessageHeader Decode(const EncodedMessageHeader& encoded);

private:
  MessageLength length_;
  MessageProperties properties_;
};

}
