#include <pep/storagefacility/PendingBytesLimiter.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <ranges>
#include <vector>

#include <prometheus/gauge.h>

using namespace std::ranges;

namespace {

/// Counts how often it is paused/resumed.
class FakeThrottle : public pep::messaging::ReadThrottle {
public:
  void pause() override {
    if (!paused) {
      paused = true;
      ++pauses;
    }
  }
  void resume() override {
    if (paused) {
      paused = false;
      ++resumes;
    }
  }

  bool paused = false;
  unsigned pauses = 0, resumes = 0;
};

constexpr uint64_t High = 100, Low = 40;

TEST(PendingBytesLimiter, DoesNotThrottleBelowHighWatermark) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  auto throttle = std::make_shared<FakeThrottle>();
  auto attachment = limiter->attach(throttle);

  auto reservation = limiter->reserve(High - 1);
  EXPECT_EQ(limiter->pendingBytes(), High - 1);
  EXPECT_FALSE(limiter->isThrottling()) << "We should not throttle before reaching the high water mark";
  EXPECT_FALSE(throttle->paused);
}

TEST(PendingBytesLimiter, PausesAtHighWatermarkAndResumesAtLowWatermark) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  auto throttle = std::make_shared<FakeThrottle>();
  auto attachment = limiter->attach(throttle);

  auto reservations = views::iota(0, 10)
    | views::transform([&](auto) {
      // Also verifies that reservations survive being moved into the vector
      return limiter->reserve(10);
    }) | to<std::vector>();
  EXPECT_EQ(limiter->pendingBytes(), 100);
  EXPECT_TRUE(limiter->isThrottling()) << "We should throttle above the high water mark";
  EXPECT_TRUE(throttle->paused);

  // Hysteresis: we must not resume until we're down to the low watermark
  for (unsigned i = 0; i < 5; ++i) { // 100 -> 50
    reservations.at(i).release();
  }
  EXPECT_EQ(limiter->pendingBytes(), 50);
  EXPECT_TRUE(limiter->isThrottling()) << "We should not stop throttling before reaching the low water mark again";
  EXPECT_TRUE(throttle->paused);

  reservations.at(5).release(); // 40
  EXPECT_EQ(limiter->pendingBytes(), Low);
  EXPECT_FALSE(limiter->isThrottling()) << "We should stop throttling after reaching the low water mark again";
  EXPECT_FALSE(throttle->paused);

  EXPECT_EQ(throttle->pauses, 1);
  EXPECT_EQ(throttle->resumes, 1);
}

TEST(PendingBytesLimiter, ThrottlesAgainAfterResuming) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  auto throttle = std::make_shared<FakeThrottle>();
  auto attachment = limiter->attach(throttle);

  auto first = limiter->reserve(High);
  ASSERT_TRUE(throttle->paused);
  first.release();
  ASSERT_FALSE(throttle->paused);

  auto second = limiter->reserve(High);
  EXPECT_TRUE(throttle->paused);
  EXPECT_EQ(throttle->pauses, 2);
}

TEST(PendingBytesLimiter, ReleasesOnlyOnceAndWhenDestroyed) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);

  std::optional reservation = limiter->reserve(30);
  std::optional other = limiter->reserve(20);
  EXPECT_EQ(limiter->pendingBytes(), 50);

  reservation->release();
  reservation->release(); // Must not release a second time
  EXPECT_EQ(limiter->pendingBytes(), 20);
  reservation.reset(); // Destroying a released reservation must not release again either
  EXPECT_EQ(limiter->pendingBytes(), 20);

  other.reset(); // Destroying a reservation that wasn't released explicitly must release it
  EXPECT_EQ(limiter->pendingBytes(), 0);
}

TEST(PendingBytesLimiter, LimitIsGlobalAcrossThrottles) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  auto throttle1 = std::make_shared<FakeThrottle>();
  auto throttle2 = std::make_shared<FakeThrottle>();
  auto attachment1 = limiter->attach(throttle1);
  auto attachment2 = limiter->attach(throttle2);

  // Neither of the two "requests" exceeds the limit by itself, but together they do
  std::optional fromFirst = limiter->reserve(60);
  EXPECT_FALSE(throttle1->paused);
  EXPECT_FALSE(throttle2->paused);
  std::optional fromSecond = limiter->reserve(60);
  EXPECT_TRUE(throttle1->paused);
  EXPECT_TRUE(throttle2->paused);

  fromFirst.reset(); // 60 remain: above the low watermark
  EXPECT_TRUE(throttle1->paused);
  fromSecond.reset();
  EXPECT_FALSE(throttle1->paused);
  EXPECT_FALSE(throttle2->paused);
}

TEST(PendingBytesLimiter, PausesThrottleAttachedWhileThrottling) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  std::optional reservation = limiter->reserve(High);
  ASSERT_TRUE(limiter->isThrottling());

  auto throttle = std::make_shared<FakeThrottle>();
  auto attachment = limiter->attach(throttle);
  EXPECT_TRUE(throttle->paused);

  reservation.reset();
  EXPECT_FALSE(throttle->paused);
}

TEST(PendingBytesLimiter, DetachingResumesThrottleAndStopsTouchingIt) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  auto throttle = std::make_shared<FakeThrottle>();
  auto attachment = limiter->attach(throttle);

  std::optional reservation = limiter->reserve(High);
  ASSERT_TRUE(throttle->paused);

  attachment.reset(); // E.g. because the request finished
  EXPECT_FALSE(throttle->paused);

  // A detached throttle is no longer affected
  reservation.reset();
  auto again = limiter->reserve(High);
  EXPECT_FALSE(throttle->paused) << "A detached throttle should not be affected";
  EXPECT_EQ(throttle->pauses, 1);
  EXPECT_EQ(throttle->resumes, 1);
}

TEST(PendingBytesLimiter, AcceptsMissingThrottle) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);
  EXPECT_EQ(limiter->attach(nullptr), nullptr);
  auto reservation = limiter->reserve(High);
  EXPECT_TRUE(limiter->isThrottling()); // Still counts, so that other throttles are affected
}

TEST(PendingBytesLimiter, ReservationCanOutliveTheAttachmentsAndCallersReferenceToLimiter) {
  std::optional<pep::PendingBytesLimiter::Reservation> reservation;
  {
    auto limiter = pep::PendingBytesLimiter::Create(High, Low);
    reservation = limiter->reserve(10);
  }
  reservation.reset(); // Must not use a destroyed limiter
}

TEST(PendingBytesLimiter, UpdatesGauges) {
  prometheus::Gauge pending, paused;
  auto limiter = pep::PendingBytesLimiter::Create(High, Low, pep::PendingBytesLimiter::Gauges{.pendingBytes = &pending, .paused = &paused});
  EXPECT_EQ(pending.Value(), 0.0);
  EXPECT_EQ(paused.Value(), 0.0);

  std::optional reservation = limiter->reserve(High);
  EXPECT_EQ(pending.Value(), static_cast<double>(High));
  EXPECT_EQ(paused.Value(), 1.0);

  reservation.reset();
  EXPECT_EQ(pending.Value(), 0.0);
  EXPECT_EQ(paused.Value(), 0.0);
}

TEST(PendingBytesLimiter, MovedReservationIsReleasedOnlyOnce) {
  auto limiter = pep::PendingBytesLimiter::Create(High, Low);

  auto original = limiter->reserve(30);
  auto moved = std::move(original);
  EXPECT_EQ(limiter->pendingBytes(), 30);
  original.release(); // Moved-from: must have no effect
  EXPECT_EQ(limiter->pendingBytes(), 30);

  auto other = limiter->reserve(20);
  EXPECT_EQ(limiter->pendingBytes(), 50);
  // Assigning over a reservation releases what that one held
  other = std::move(moved);
  EXPECT_EQ(limiter->pendingBytes(), 30);

  other = pep::PendingBytesLimiter::Reservation{}; // Assigning an empty one releases too
  EXPECT_EQ(limiter->pendingBytes(), 0);
}

TEST(PendingBytesLimiter, RejectsInvalidWatermarks) {
  EXPECT_THROW(pep::PendingBytesLimiter::Create(0U, 0U), std::invalid_argument);
  EXPECT_THROW(pep::PendingBytesLimiter::Create(10U, 10U), std::invalid_argument);
  EXPECT_THROW(pep::PendingBytesLimiter::Create(10U, 20U), std::invalid_argument);
}

}
