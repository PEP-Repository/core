#include <pep/messaging/RequestHandler.hpp>

namespace pep::messaging {

MessageBatches RequestHandler::handleRequest(MessageMagic magic, std::shared_ptr<std::string> message, MessageSequence tail) {
  auto position = methods_.find(magic);
  if (position == methods_.cend()) {
    throw Error("Unsupported message type " + DescribeMessageMagic(magic));
  }
  return position->second->handle(*this, message, tail);
}

}
