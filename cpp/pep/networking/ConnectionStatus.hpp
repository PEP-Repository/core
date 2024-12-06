#pragma once

#include <boost/system/error_code.hpp>

namespace pep {

struct ConnectionStatus {
  bool connected;
  boost::system::error_code error;
  int attempt;
  int retryTimeout;
  time_t time;

  explicit inline ConnectionStatus() : ConnectionStatus(true, make_error_code(boost::system::errc::success), 0, 0) {}

  ConnectionStatus(bool connected, boost::system::error_code error, int attempt, int retryTimeout)
    : connected(connected), error(error), attempt(attempt), retryTimeout(retryTimeout), time(std::time(nullptr)) {}
};

}
