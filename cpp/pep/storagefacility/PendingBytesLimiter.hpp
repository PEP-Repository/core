#pragma once

#include <pep/messaging/ReadThrottle.hpp>
#include <pep/utils/Shared.hpp>

#include <cstdint>
#include <memory>
#include <unordered_set>

#include <boost/noncopyable.hpp>

namespace prometheus {
class Gauge;
}

namespace pep {

/// \brief Bounds the amount of data that we've accepted from the network but haven't finished processing yet.
/// \details Incoming pages are received (from a socket) much faster than a slow backend (e.g. S3) may be able to store them. Without a limit, the surplus
///   piles up in memory. Code that receives data \c reserve s space for it, and releases that space when the data has been dealt with (or discarded).
///   When the reserved total reaches the high watermark, the limiter pauses all attached \c messaging::ReadThrottle s, i.e. we stop reading from the
///   connections that deliver data. Reading resumes when the total drops to the low watermark. (The gap between the two prevents flapping.)
///   The limit is global: it applies to the sum of what all streams have reserved.
/// \remark Not thread-safe. Use from a single thread (the \c io_context thread that also drives the connections that are being throttled) only.
/// \remark The limit is soft: data that was already in flight when the limiter engaged (up to one message per connection) is still accepted.
class PendingBytesLimiter : public std::enable_shared_from_this<PendingBytesLimiter>, public SharedConstructor<PendingBytesLimiter> {
  friend SharedConstructor;

public:
  /// \brief Optional metrics that the limiter keeps up to date. Must outlive the limiter.
  struct Gauges {
    prometheus::Gauge* pendingBytes = nullptr;
    prometheus::Gauge* paused = nullptr; ///< 1 while throttling, otherwise 0
  };

  /// \brief Space that was reserved in a limiter, and is released when this object is released or destroyed.
  /// \remark Move-only. A default-constructed or moved-from instance reserves nothing.
  /// \remark Wrap in a \c std::shared_ptr if you need to capture it in a copyable callback.
  class Reservation {
    friend PendingBytesLimiter;

  public:
    Reservation() noexcept = default;
    Reservation(const Reservation&) = delete;
    Reservation& operator=(const Reservation&) = delete;
    Reservation(Reservation&& other) noexcept : limiter_(std::move(other.limiter_)), bytes_(other.bytes_) {}
    Reservation& operator=(Reservation&& other) noexcept;
    ~Reservation() noexcept { this->release(); }

    /// \brief Releases the reserved space. Does nothing if that has already been done.
    void release() noexcept;

  private:
    Reservation(std::shared_ptr<PendingBytesLimiter> limiter, uint64_t bytes) noexcept : limiter_(std::move(limiter)), bytes_(bytes) {}

    std::shared_ptr<PendingBytesLimiter> limiter_; // Empty if we don't (or no longer) hold a reservation
    uint64_t bytes_ = 0;
  };

  /// \brief Registration of a \c ReadThrottle with a limiter. Destroying it detaches the throttle, which resumes it.
  /// \remark Neither copyable nor movable, because the limiter refers to it by address.
  class Attachment : boost::noncopyable {
    friend PendingBytesLimiter;

  public:
    ~Attachment() noexcept;

  private:
    Attachment(std::shared_ptr<PendingBytesLimiter> limiter, std::shared_ptr<messaging::ReadThrottle> throttle) noexcept
      : limiter_(std::move(limiter)), throttle_(std::move(throttle)) {}

    std::shared_ptr<PendingBytesLimiter> limiter_;
    std::shared_ptr<messaging::ReadThrottle> throttle_;
  };

  /// \brief Accounts for data that has been received and is now waiting to be processed. May pause reading.
  /// \return An object that releases the reservation when it's destroyed (or explicitly released). Ensure that this happens exactly when
  ///   the data has been processed or discarded: if it stays alive, the limiter will eventually stop all reading forever.
  [[nodiscard]] Reservation reserve(uint64_t bytes);

  /// \brief Subjects a throttle to the limiter: it will be paused while the limiter is throttling (also if that is already the case now).
  /// \param throttle May be nullptr, in which case nothing will be throttled.
  /// \return An object that detaches the throttle (and resumes it) when destroyed, or nullptr if \p throttle was nullptr.
  [[nodiscard]] std::unique_ptr<Attachment> attach(std::shared_ptr<messaging::ReadThrottle> throttle);

  uint64_t pendingBytes() const noexcept { return pending_; }
  bool isThrottling() const noexcept { return throttling_; }

private:
  /// \param highWatermark Reading pauses when at least this many bytes are reserved. Must not be 0.
  /// \param lowWatermark Reading resumes when at most this many bytes remain reserved. Must be less than \p highWatermark.
  PendingBytesLimiter(uint64_t highWatermark, uint64_t lowWatermark) : PendingBytesLimiter(highWatermark, lowWatermark, Gauges{}) {}
  PendingBytesLimiter(uint64_t highWatermark, uint64_t lowWatermark, Gauges gauges);

  void release(uint64_t bytes) noexcept;
  void detach(Attachment& attachment) noexcept;
  void setThrottling(bool throttling) noexcept;
  void updateGauges() noexcept;

  const uint64_t high_;
  const uint64_t low_;
  const Gauges gauges_;

  uint64_t pending_ = 0;
  bool throttling_ = false;
  std::unordered_set<Attachment*> attachments_;
};

}
