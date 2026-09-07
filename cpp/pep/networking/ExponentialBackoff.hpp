#pragma once

#include <pep/async/IoContext_fwd.hpp>
#include <boost/asio/steady_timer.hpp>

namespace pep {

class ExponentialBackoff {
public:
  using Timer = boost::asio::steady_timer;
  using Timeout = Timer::duration;
  using BackoffFactor = Timeout::rep;

  class Parameters {
  private:
    Timeout minTimeout_;
    Timeout maxTimeout_;
    BackoffFactor backoffFactor_;

  public:
    Parameters(Timeout minTimeout = std::chrono::seconds{ 1 }, Timeout maxTimeout = std::chrono::minutes{ 5 }, BackoffFactor backoffFactor = 2) noexcept;

    Timeout minTimeout() const noexcept { return minTimeout_; }
    Timeout maxTimeout() const noexcept { return maxTimeout_; }
    BackoffFactor backoffFactor() const noexcept { return backoffFactor_; }
  };

  using RetryHandler = std::function<void(const boost::system::error_code&)>;

  explicit ExponentialBackoff(boost::asio::io_context& io_context, Parameters parameters = Parameters())
    : timer_(io_context), parameters_(parameters), nextTimeout_(parameters_.minTimeout()) {}

  // retry the RetryHandler, with exponential backoff. Returns the timeout or nullopt if already queued.
  std::optional<Timeout> retry(RetryHandler handler);

  //signal success
  void success();

  void stop();

private:
  Timer timer_;
  Parameters parameters_;
  Timeout nextTimeout_;
};

}
