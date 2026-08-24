#include <pep/networking/Protocol.hpp>

#include <pep/networking/Connection.hpp>

namespace pep::networking {

std::string Protocol::ClientComponent::describe() const { return "client to " + this->connectionAddress(); }

std::string Protocol::ClientComponent::describeConnection(const Connection&) const {
  // The description already includes the remote server address
  return describe();
}

std::string Protocol::ServerComponent::describe() const { return "server listening at " + this->connectionAddress(); }

std::string Protocol::ServerComponent::describeConnection(const Connection& connection) const {
  return describe() + " connected to " + connection.remoteAddress();
}

}
