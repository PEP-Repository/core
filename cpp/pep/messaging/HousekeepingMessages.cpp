#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

#include <random>

namespace pep {

PingRequest::PingRequest()
  : id_{ RandomInteger<decltype(id_)>() } {}

void PingResponse::validate(const PingRequest& isReplyTo) const {
  if (id_ != isReplyTo.id()) {
    throw std::runtime_error("Ping response does not match the request");
  }
}

}
