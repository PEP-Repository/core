#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/utils/Random.hpp>

namespace pep {

PingRequest::PingRequest() {
  RandomBytes(reinterpret_cast<uint8_t*>(&mId), sizeof(mId));
}

}
