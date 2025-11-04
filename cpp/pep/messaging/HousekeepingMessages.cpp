#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

namespace pep {

PingRequest::PingRequest(): mId{/*placeholder*/} {
  RandomBytes(reinterpret_cast<uint8_t*>(&mId), sizeof(mId));
}

void PingResponse::validate(const PingRequest& isReplyTo) const {
  if (mId != isReplyTo.mId) {
    throw std::runtime_error("Ping response does not match the request");
  }
}

}
