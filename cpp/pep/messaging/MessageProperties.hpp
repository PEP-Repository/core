#pragma once

#include <pep/utils/Attributes.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <pep/utils/TypeTraits.hpp>

#include <bit>
#include <cassert>
#include <compare>
#include <cstdint>
#include <ostream>

namespace pep::messaging {

// Every message is sent and received with some properties encoded into a single integral value
using EncodedMessageProperties = uint32_t;

namespace detail::encoding_layout {

constexpr EncodedMessageProperties TypeBits = 0x8000'0000; // single high bit to indicate message type
constexpr EncodedMessageProperties FlagBits = 0x7000'0000; // (the next-highest) three bits for state-related flags
constexpr EncodedMessageProperties StreamIdBits = ~(TypeBits | FlagBits); // remaining bits for a unique (serial) number for every request+response cycle

} // namespace encoding

// The (single) high bit in EncodedMessageProperties indicates message type
class MessageType {
public:
  enum Value { // Intentionally not an enum _class_ so we can write e.g. "MessageType::Control"
    Control,
    Request,
    Response
  };

  static bool IsValidValue(Value value) noexcept;

  MessageType(Value value);
  std::strong_ordering operator<=>(const MessageType& other) const = default;

  Value value() const noexcept { return value_; }
  std::string describe() const;

  EncodedMessageProperties encode() const noexcept;

private:
  Value value_;
};

// (The next-highest) three bits in EncodedMessageProperties are used for state-related flags
class Flags final {
  constexpr static auto Shift = std::countr_zero(detail::encoding_layout::FlagBits);

public:
  enum class PEP_ATTRIBUTE_FLAG_ENUM Bits : EncodedMessageProperties {
    None    = 0,
    Close   = 0b100 << Shift, ///< This is the last piece of the (possibly multi-part) message
    Error   = 0b010 << Shift, ///< The sending party encountered an error. Implies Close.
    Payload = 0b001 << Shift, ///< The message includes content
    All     = 0b111 << Shift,
  };

  const static Flags None;
  const static Flags Close;
  const static Flags Error;
  const static Flags Payload;
  const static Flags ClosingPayload;

  [[nodiscard]] Flags withClose() const;
  [[nodiscard]] bool has(Flags subset) const noexcept;

  [[nodiscard]] EncodedMessageProperties encode() const noexcept { return ToUnderlying(bits_); }

  [[nodiscard]] static Flags DecodeFrom(EncodedMessageProperties properties) noexcept {
    return Flags(Bits(static_cast<EncodedMessageProperties>(properties & detail::encoding_layout::FlagBits)));
  }

  std::strong_ordering operator <=>(const Flags&) const noexcept = default;

  static void EnsureValid(Flags f) { EnsureValid(f.bits_); }

private:
  /// Throws std::invalid_argument if the combination of bits is not valid
  explicit Flags(Bits bits): bits_(EnsureValid(bits)) {};

  /// Identity function that throws std::invalid_argument if the combination of bits is not valid
  static Bits EnsureValid(Bits);

  Bits bits_;
};

} // namespace pep::messaging

PEP_MARK_AS_FLAG_ENUM_TYPE(::pep::messaging::Flags::Bits)

namespace pep::messaging {

inline const Flags Flags::None = Flags{Flags::Bits::None};
inline const Flags Flags::Close = Flags{Flags::Bits::Close};
inline const Flags Flags::Error = Flags{Flags::Bits::Error | Flags::Bits::Close};
inline const Flags Flags::Payload = Flags{Flags::Bits::Payload};
inline const Flags Flags::ClosingPayload = Flags{Flags::Bits::Payload | Flags::Bits::Close};

std::ostream& operator<<(std::ostream& out, Flags::Bits flags);

inline bool Flags::has(Flags subset) const noexcept{ return HasFlags(bits_, subset.bits_); }
inline Flags Flags::withClose() const { return Flags{bits_ | Flags::Bits::Close}; }

// Remaining bits in EncodedMessageProperties represent a unique (serial) number for every request+response cycle
class StreamId {
public:
  using Value = EncodedMessageProperties;
  static bool IsValidValue(Value value) noexcept;

  explicit StreamId(Value value);
  std::strong_ordering operator<=>(const StreamId& other) const = default;

  Value value() const noexcept { return value_; }

  EncodedMessageProperties encode() const noexcept { return this->value(); }

  static StreamId BeforeFirst() noexcept;
  static StreamId MakeNext(const StreamId& previous) noexcept;

private:
  Value value_;
};

inline std::ostream& operator<<(std::ostream& lhs, const StreamId& rhs) { return lhs << rhs.value(); }

// A MessageId is the combination of the StreamId and the message type: allows us to distinguish between our request NNN, and our response to someone else's request NNN
class MessageId {
public:
  MessageId(MessageType type, StreamId streamId);
  std::strong_ordering operator<=>(const MessageId& other) const = default;

  static MessageId MakeForControlMessage() noexcept;

  MessageType type() const noexcept { return type_; }
  const StreamId& streamId() const noexcept { return streamId_; }

  EncodedMessageProperties encode() const noexcept;

private:
  MessageType type_;
  StreamId streamId_;
};

class MessageProperties {
public:
  MessageProperties(MessageId messageId, Flags flags);

  static MessageProperties MakeForControlMessage() noexcept;

  const MessageId& messageId() const noexcept { return messageId_; }
  const Flags& flags() const noexcept { return flags_; }

  EncodedMessageProperties encode() const noexcept;
  static MessageProperties DecodeFrom(EncodedMessageProperties properties);

private:
  MessageId messageId_;
  Flags flags_;
};

}
