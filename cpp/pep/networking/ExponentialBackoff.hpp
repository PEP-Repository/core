#pragma once

#include <boost/asio.hpp>


#include <cmath>

namespace pep {

class ExponentialBackoff {
  const static int CONNECT_BACKOFF_FACTOR = 2; // backoff becomes twice as large each attempt
  const static int CONNECT_MAX_TIMEOUT = 5 * 60;
  const static int CONNECT_MIN_TIMEOUT = 1;

  boost::asio::deadline_timer timer;
  int nextTimeout = CONNECT_MIN_TIMEOUT;

 public:
  explicit ExponentialBackoff(boost::asio::io_service& io_service)
    : timer(io_service) {}

  // retry the retryhandler, with exponential backoff. Returns the timeout.
  template<typename RetryHandler>
  int retry(RetryHandler handler) {
    if(timer.expires_from_now() > boost::posix_time::seconds(0)) { //Do not retry if a retry is already queued
      return -1;
    }
    int timeout = nextTimeout;
    nextTimeout *= CONNECT_BACKOFF_FACTOR;
    if (nextTimeout > ExponentialBackoff::CONNECT_MAX_TIMEOUT)
      nextTimeout = ExponentialBackoff::CONNECT_MAX_TIMEOUT;

    timer.expires_from_now(boost::posix_time::seconds(timeout));
    timer.async_wait(handler);
    return timeout;
  }
  //signal success
  void success() {
    nextTimeout = CONNECT_MIN_TIMEOUT; // Everything went fine, so no need to further blow up reconnection times
  }
};

}
