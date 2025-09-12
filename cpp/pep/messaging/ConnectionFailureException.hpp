#pragma once

#include <boost/system/errc.hpp>

namespace pep::messaging {

class ConnectionFailureException : public std::runtime_error {
private:
  boost::system::errc::errc_t mReason;

public:
  ConnectionFailureException(boost::system::errc::errc_t reason, const std::string& message) noexcept
    : std::runtime_error(message), mReason(reason) {}

  boost::system::errc::errc_t getReason() const noexcept { return mReason; }

  static ConnectionFailureException ForVersionCheckFailure(const std::string& message) { return ConnectionFailureException(boost::system::errc::wrong_protocol_type, message); }
};


}
