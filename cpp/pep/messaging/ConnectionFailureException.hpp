#pragma once

#include <boost/system/errc.hpp>

namespace pep::messaging {

class ConnectionFailureException : public std::runtime_error {
private:
  boost::system::errc::errc_t reason_;

public:
  ConnectionFailureException(boost::system::errc::errc_t reason, const std::string& message) noexcept
    : std::runtime_error(message), reason_(reason) {}

  boost::system::errc::errc_t getReason() const noexcept { return reason_; }

  static ConnectionFailureException ForVersionCheckFailure(const std::string& message) { return ConnectionFailureException(boost::system::errc::wrong_protocol_type, message); }
};


}
