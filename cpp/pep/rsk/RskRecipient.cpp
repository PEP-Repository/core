#include <pep/rsk/RskRecipient.hpp>

#include <cassert>
#include <stdexcept>
#include <utility>

using namespace pep;

RecipientBase::RecipientBase(Type type, std::string payload)
    : type_(type), payload_(std::move(payload)) {
  if (!type_) {
    throw std::invalid_argument("Recipient type must be nonzero");
  }
  if (payload_.empty()) {
    throw std::invalid_argument("Recipient payload cannot be empty");
  }
}

ReshuffleRecipient::ReshuffleRecipient(Type type, std::string payload)
    : RecipientBase(type, std::move(payload)) {}

RekeyRecipient::RekeyRecipient(Type type, std::string payload)
    : RecipientBase(type, std::move(payload)) {}

SkRecipient::SkRecipient(Type type, Payload payload)
    : ReshuffleRecipient(type, std::move(payload.reshuffle)),
      RekeyRecipient(type, std::move(payload.rekey)) {}

SkRecipient::Type SkRecipient::type() const noexcept {
  assert(ReshuffleRecipient::type() == RekeyRecipient::type());
  return ReshuffleRecipient::type();
}
