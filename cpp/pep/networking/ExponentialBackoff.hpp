#pragma once

#include <pep/async/IoContext_fwd.hpp>
#include <boost/asio/steady_timer.hpp>

namespace pep {

class ExponentialBackoff {
  boost::asio::steady_timer timer;

  static constexpr decltype(timer)::duration::rep
    CONNECT_BACKOFF_FACTOR = 2; // backoff becomes twice as large each attempt
  static constexpr decltype(timer)::duration
    CONNECT_MAX_TIMEOUT = std::chrono::minutes{5},
    CONNECT_MIN_TIMEOUT = std::chrono::seconds{1};

  decltype(timer)::duration nextTimeout = CONNECT_MIN_TIMEOUT;

 public:
  explicit ExponentialBackoff(boost::asio::io_context& io_context)
    : timer(io_context) {}

  // retry the RetryHandler, with exponential backoff. Returns the timeout or nullopt if already queued.
  template<typename RetryHandler>
  std::optional<decltype(timer)::duration> retry(RetryHandler handler) {
    if(timer.expiry() > decltype(timer)::clock_type::now()) { //Do not retry if a retry is already queued
      return {};
    }
    auto timeout = std::exchange(nextTimeout,
        std::min(nextTimeout * CONNECT_BACKOFF_FACTOR, CONNECT_MAX_TIMEOUT));
    timer.expires_after(timeout);
    timer.async_wait(handler);
    return timeout;
  }
  //signal success
  void success() {
    nextTimeout = CONNECT_MIN_TIMEOUT; // Everything went fine, so no need to further blow up reconnection times
  }
};

}
