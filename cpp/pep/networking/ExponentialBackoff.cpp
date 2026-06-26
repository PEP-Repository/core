#include <pep/networking/ExponentialBackoff.hpp>

namespace pep {

ExponentialBackoff::Parameters::Parameters(Timeout minTimeout, Timeout maxTimeout, BackoffFactor backoffFactor) noexcept
  : minTimeout_(minTimeout), maxTimeout_(maxTimeout), backoffFactor_(backoffFactor) {
  assert(minTimeout_ > Timeout(0));
  assert(maxTimeout_ > minTimeout_);
  assert(backoffFactor_ > 1);
}

std::optional<ExponentialBackoff::Timeout> ExponentialBackoff::retry(ExponentialBackoff::RetryHandler handler) {
  if (timer_.expiry() > Timer::clock_type::now()) { //Do not retry if a retry is already queued
    return {};
  }
  auto timeout = std::exchange(nextTimeout_,
    std::min(nextTimeout_ * parameters_.backoffFactor(), parameters_.maxTimeout()));
  timer_.expires_after(timeout);
  timer_.async_wait(handler);
  return timeout;
}

void ExponentialBackoff::success() {
  nextTimeout_ = parameters_.minTimeout(); // Everything went fine, so no need to further blow up reconnection times
}

void ExponentialBackoff::stop() {
  timer_.cancel();
  nextTimeout_ = parameters_.minTimeout();
}

}
