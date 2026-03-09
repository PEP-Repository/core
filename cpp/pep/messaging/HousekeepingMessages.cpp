#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

namespace pep {

PingRequest::PingRequest(): mId{/*placeholder*/} {
  mId = std::uniform_int_distribution<decltype(mId)>{}(ThreadUrbg);
}

void PingResponse::validate(const PingRequest& isReplyTo) const {
  if (mId != isReplyTo.mId) {
    throw std::runtime_error("Ping response does not match the request");
  }
}

}
