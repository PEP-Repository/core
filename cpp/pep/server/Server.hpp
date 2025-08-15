#pragma once

#include <pep/networking/TLSMessageServer.hpp>
#include <pep/server/MonitorableServer.hpp>

namespace pep {

class Server : public MonitorableServer<TLSMessageServer> {
protected:
  explicit Server(std::shared_ptr<Parameters> parameters)
    : MonitorableServer<TLSMessageServer>(parameters) {
  }
};

class SigningServer : public MonitorableServer<TLSSignedMessageServer> {
protected:
  explicit SigningServer(std::shared_ptr<Parameters> parameters)
    : MonitorableServer<TLSSignedMessageServer>(parameters) {
  }
};

}
