#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

#include <random>

namespace pep {

PingRequest::PingRequest()
  : mId{ CryptoUrbg{}() } {
  static_assert(std::is_same_v<decltype(mId), CryptoUrbg::result_type>, "Need a random value for mId's full range");
}

void PingResponse::validate(const PingRequest& isReplyTo) const {
  if (mId != isReplyTo.mId) {
    throw std::runtime_error("Ping response does not match the request");
  }
}

}
