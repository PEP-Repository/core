#pragma once

#include <boost/system/error_code.hpp>

namespace pep {

struct ConnectionStatus {
  bool connected;
  boost::system::error_code error;

  explicit inline ConnectionStatus() : ConnectionStatus(true, make_error_code(boost::system::errc::success)) {}

  ConnectionStatus(bool connected, boost::system::error_code error)
    : connected(connected), error(error) {}
};

}
