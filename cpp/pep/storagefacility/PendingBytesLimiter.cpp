#include <pep/storagefacility/PendingBytesLimiter.hpp>
#include <pep/utils/Log.hpp>

#include <prometheus/gauge.h>

#include <cassert>
#include <stdexcept>

namespace pep {

namespace {

const std::string LogTag("PendingBytesLimiter");

}

PendingBytesLimiter::PendingBytesLimiter(uint64_t highWatermark, uint64_t lowWatermark, Gauges gauges)
  : high_(highWatermark), low_(lowWatermark), gauges_(gauges) {
  if (high_ == 0) {
    throw std::invalid_argument("PendingBytesLimiter's high watermark cannot be 0");
  }
  if (low_ >= high_) {
    throw std::invalid_argument("PendingBytesLimiter's low watermark must be less than its high watermark");
  }
  this->updateGauges();
}

PendingBytesLimiter::Reservation PendingBytesLimiter::reserve(uint64_t bytes) {
  Reservation result(this->shared_from_this(), bytes);

  pending_ += bytes;
  this->updateGauges();
  if (!throttling_ && pending_ >= high_) {
    this->setThrottling(true);
  }
  return result;
}

void PendingBytesLimiter::release(uint64_t bytes) noexcept {
  assert(pending_ >= bytes);
  pending_ -= bytes;
  this->updateGauges();
  if (throttling_ && pending_ <= low_) {
    this->setThrottling(false);
  }
}

PendingBytesLimiter::Reservation& PendingBytesLimiter::Reservation::operator=(Reservation&& other) noexcept {
  if (this != &other) {
    this->release(); // Release what this instance held previously
    limiter_ = std::move(other.limiter_);
    bytes_ = other.bytes_;
  }
  return *this;
}

void PendingBytesLimiter::Reservation::release() noexcept {
  if (limiter_ != nullptr) {
    std::exchange(limiter_, nullptr)->release(bytes_);
  }
}

std::unique_ptr<PendingBytesLimiter::Attachment> PendingBytesLimiter::attach(std::shared_ptr<messaging::ReadThrottle> throttle) {
  if (throttle == nullptr) {
    return nullptr;
  }

  // Not std::make_unique because of the private constructor
  std::unique_ptr<Attachment> result(new Attachment(this->shared_from_this(), std::move(throttle)));
  attachments_.insert(result.get());
  // Immediately pause when already throttling
  if (throttling_) {
    result->throttle_->pause();
  }
  return result;
}

void PendingBytesLimiter::detach(Attachment& attachment) noexcept {
  attachments_.erase(&attachment);
}

PendingBytesLimiter::Attachment::~Attachment() noexcept {
  limiter_->detach(*this);
  throttle_->resume(); // Does nothing if it wasn't paused
}

void PendingBytesLimiter::setThrottling(bool throttling) noexcept {
  assert(throttling != throttling_);
  throttling_ = throttling;
  this->updateGauges();

  if (throttling) {
    PEP_LOG(LogTag, Severity::Debug) << "Pausing reading of incoming data because " << pending_ << " bytes are still pending (limit " << high_ << ")";
  } else {
    PEP_LOG(LogTag, Severity::Debug) << "Resuming reading of incoming data because only " << pending_ << " bytes are still pending (limit " << low_ << ")";
  }

  // Pausing and resuming only changes state of the throttled connection: it doesn't call back into us, so we can't be modified while iterating.
  for (Attachment* attachment : attachments_) {
    if (throttling) {
      attachment->throttle_->pause();
    } else {
      attachment->throttle_->resume();
    }
  }
}

void PendingBytesLimiter::updateGauges() noexcept {
  if (gauges_.pendingBytes != nullptr) {
    gauges_.pendingBytes->Set(static_cast<double>(pending_));
  }
  if (gauges_.paused != nullptr) {
    gauges_.paused->Set(throttling_ ? 1.0 : 0.0);
  }
}

}
