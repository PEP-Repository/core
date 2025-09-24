#include <pep/networking/ExponentialBackoff.hpp>

namespace pep {

ExponentialBackoff::Parameters::Parameters(Timeout minTimeout, Timeout maxTimeout, BackoffFactor backoffFactor) noexcept
  : mMinTimeout(minTimeout), mMaxTimeout(maxTimeout), mBackoffFactor(backoffFactor) {
  assert(mMinTimeout > Timeout(0));
  assert(mMaxTimeout > mMinTimeout);
  assert(mBackoffFactor > 1);
}

std::optional<ExponentialBackoff::Timeout> ExponentialBackoff::retry(ExponentialBackoff::RetryHandler handler) {
  if (mTimer.expiry() > Timer::clock_type::now()) { //Do not retry if a retry is already queued
    return {};
  }
  auto timeout = std::exchange(mNextTimeout,
    std::min(mNextTimeout * mParameters.backoffFactor(), mParameters.maxTimeout()));
  mTimer.expires_after(timeout);
  mTimer.async_wait(handler);
  return timeout;
}

void ExponentialBackoff::success() {
  mNextTimeout = mParameters.minTimeout(); // Everything went fine, so no need to further blow up reconnection times
}

void ExponentialBackoff::stop() {
  mTimer.cancel();
  mNextTimeout = mParameters.minTimeout();
}

}
