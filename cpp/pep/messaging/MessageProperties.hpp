#pragma once

#include <cassert>
#include <compare>
#include <cstdint>
#include <ostream>

namespace pep::messaging {

// Every message is sent and received with some properties encoded into a single integral value
using EncodedMessageProperties = uint32_t;


// The (single) high bit in EncodedMessageProperties indicates message type
class MessageType {
public:
  enum Value {
    CONTROL,
    REQUEST,
    RESPONSE
  };

  static bool IsValidValue(Value value) noexcept;

  MessageType(Value value);
  std::strong_ordering operator<=>(const MessageType& other) const = default;

  Value value() const noexcept { return mValue; }
  std::string describe() const;

  EncodedMessageProperties encode() const noexcept;

private:
  Value mValue;
};


// (The next-highest) three bits in EncodedMessageProperties are used for state-related flags
class Flags {
public:
  Flags(bool close, bool error, bool payload);
  std::strong_ordering operator<=>(const Flags& other) const = default;

  static Flags MakeEmpty() noexcept;
  static Flags MakeError() noexcept;
  static Flags MakePayload(bool close = false) noexcept;
  static Flags MakeClose(bool payload = false) noexcept;

  bool empty() const noexcept; // Is any flag set?

  bool close() const noexcept { return mClose; } // This is the last piece of the (possibly multi-part) message
  bool error() const noexcept { return mError; } // The sending party encountered an error constructing or sending the (possibly multi-part) message. Implies Flags::close()
  bool payload() const noexcept { return mPayload; } // The message includes content

  Flags operator|(const Flags& other) const;

  EncodedMessageProperties encode() const noexcept;

  friend std::ostream& operator<<(std::ostream& out, Flags flags);

private:
  [[nodiscard]] bool areValid() const noexcept;

  bool mClose;
  bool mError;
  bool mPayload;
};


// Remaining bits in EncodedMessageProperties represent a unique (serial) number for every request+response cycle
class StreamId {
public:
  using Value = EncodedMessageProperties;
  static bool IsValidValue(Value value) noexcept;

  explicit StreamId(Value value);
  std::strong_ordering operator<=>(const StreamId& other) const = default;

  Value value() const noexcept { return mValue; }

  EncodedMessageProperties encode() const noexcept { return this->value(); }

  static StreamId BeforeFirst() noexcept;
  static StreamId MakeNext(const StreamId& previous) noexcept;

private:
  Value mValue;
};

inline std::ostream& operator<<(std::ostream& lhs, const StreamId& rhs) { return lhs << rhs.value(); }


// A MessageId is the combination of the StreamId and the message type: allows us to distinguish between our request NNN, and our response to someone else's request NNN
class MessageId {
public:
  MessageId(MessageType type, StreamId streamId);
  std::strong_ordering operator<=>(const MessageId& other) const = default;

  static MessageId MakeForControlMessage() noexcept;

  MessageType type() const noexcept { return mType; }
  const StreamId& streamId() const noexcept { return mStreamId; }

  EncodedMessageProperties encode() const noexcept;

private:
  MessageType mType;
  StreamId mStreamId;
};


class MessageProperties {
public:
  MessageProperties(MessageId messageId, Flags flags);

  static MessageProperties MakeForControlMessage() noexcept;

  const MessageId& messageId() const noexcept { return mMessageId; }
  const Flags& flags() const noexcept { return mFlags; }

  EncodedMessageProperties encode() const noexcept;
  static MessageProperties DecodeFrom(EncodedMessageProperties properties);

private:
  MessageId mMessageId;
  Flags mFlags;
};


}
