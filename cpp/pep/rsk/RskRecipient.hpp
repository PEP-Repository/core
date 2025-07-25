#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pep {

class RecipientBase {
public:
  using Type = uint32_t;

  [[nodiscard]] Type type() const { return type_; }
  [[nodiscard]] std::string_view payload() const { return payload_; }

protected:
  RecipientBase(Type type, std::string payload);

private:
  Type type_;
  std::string payload_;
};

class ReshuffleRecipient : public RecipientBase {
public:
  ReshuffleRecipient(Type type, std::string payload);
};

class RekeyRecipient : public RecipientBase {
public:
  RekeyRecipient(Type type, std::string payload);
};

/// reShuffle & reKey recipient
class SkRecipient : public ReshuffleRecipient, public RekeyRecipient {
public:
  struct Payload {
    std::string reshuffle, rekey;
  };

  SkRecipient(Type type, Payload payload);

  [[nodiscard]] Type type() const noexcept /*override*/;
};

} // namespace pep
