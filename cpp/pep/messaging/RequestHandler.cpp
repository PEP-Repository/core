#include <pep/messaging/RequestHandler.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <pep/messaging/ReadThrottle.hpp>
#include <pep/serialization/MessageMagic.hpp>

#include <memory>
#include <string>
#include <utility>

namespace pep::messaging {

MessageBatches RequestHandler::handleRequest(MessageMagic magic, std::shared_ptr<std::string> message, MessageSequence tail, std::shared_ptr<ReadThrottle> throttle) {
  auto position = methods_.find(magic);
  if (position == methods_.cend()) {
    throw Error("Unsupported message type " + DescribeMessageMagic(magic));
  }
  return position->second->handle(*this, message, tail, std::move(throttle));
}

}
