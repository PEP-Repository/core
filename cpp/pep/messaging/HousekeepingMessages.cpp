#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

#include <random>

namespace pep {

PingRequest::PingRequest()
  : mId{ RandomInteger<decltype(mId)>() } {}

void PingResponse::validate(const PingRequest& isReplyTo) const {
  if (mId != isReplyTo.mId) {
    throw std::runtime_error("Ping response does not match the request");
  }
}

}
