#include <pep/async/ReaderWriterLock.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Timestamp.hpp>
#include <gtest/gtest.h>
#include <array>
#include <thread>

namespace {

class TimeRange {
private:
  pep::Timestamp started_, ended_;

public:
  TimeRange(pep::Timestamp started, pep::Timestamp ended) noexcept
    : started_(started), ended_(ended) {
    assert(started_ <= ended_);
  }

  TimeRange() noexcept = default;

  pep::Timestamp started() const noexcept { return started_; }
  pep::Timestamp ended() const noexcept { return ended_; }

  bool isDefaulted() const noexcept { return started_ == pep::Timestamp() && ended_ == pep::Timestamp(); }
  bool contains(pep::Timestamp when) const noexcept;
  bool isContiguousWith(const TimeRange& other) const;
  bool intersectsWith(const TimeRange& other) const;
};

bool TimeRange::contains(pep::Timestamp when) const noexcept {
  return !this->isDefaulted() && when >= started_ && when <= ended_;
}

bool TimeRange::isContiguousWith(const TimeRange& other) const {
  return this->contains(other.started_) || this->contains(other.ended_);
}

bool TimeRange::intersectsWith(const TimeRange& other) const {
  return this->isContiguousWith(other)
    && started_ != other.ended_
    && ended_ != other.started_;
}


class Contention {
private:
  bool wrote_{ false };
  TimeRange ran_;

  Contention(bool wrote, TimeRange ran) noexcept
    : wrote_(wrote), ran_(ran) {
  }

  static std::chrono::milliseconds RandomDuration();
  static TimeRange SpendSomeTime();

public:
  Contention() noexcept = default;

  bool wrote() const noexcept { return wrote_; }
  TimeRange ran() const noexcept { return ran_; }
  bool isDefaulted() const noexcept { return ran_.isDefaulted(); }

  static Contention Run(pep::ReaderWriterLock& rwl, bool write);
};

std::chrono::milliseconds Contention::RandomDuration() {
  pep::CryptoUrbg rng;
  std::uniform_int_distribution<std::chrono::milliseconds::rep> dist(0, 63);
  return std::chrono::milliseconds(dist(rng));
}

TimeRange Contention::SpendSomeTime() {
  auto duration = RandomDuration();
  auto started = pep::TimeNow();
  std::this_thread::sleep_for(duration);
  return TimeRange(started, pep::TimeNow());
}

Contention Contention::Run(pep::ReaderWriterLock& rwl, bool write) {
  return Contention(write, rwl.acquire(write, &SpendSomeTime));
}

}

TEST(ReaderWriterLock, Works) {
  constexpr size_t ThreadCount = 200;
  constexpr unsigned IntersectingReadAttempts = 5;
  constexpr unsigned ReaderToWriterRatio = 3; // One writer to this many readers: e.g. if set to 3, then 1 in 4 threads will be a writer (and 3 in 4 will be a reader)

  static_assert(ReaderToWriterRatio != 0, "Can't modulate very well otherwise, can I now?");
  constexpr unsigned ReaderToWriterDivisor = ReaderToWriterRatio + 1;

  bool intersectingReadFound = false;
  for (unsigned attempt = 0; (attempt < IntersectingReadAttempts) && !intersectingReadFound; ++attempt) {
    std::array<std::thread, ThreadCount> threads;
    std::array<Contention, ThreadCount> results;
    pep::ReaderWriterLock rwl;

    // Run "ThreadCount" threads, each of which stores (assigns) the corresponding "Contention" instance in the "results" array
    for (size_t i = 0; i < ThreadCount; ++i) {
      threads[i] = std::thread([i, &rwl, &results]() {
          assert(results[i].isDefaulted());
          results[i] = Contention::Run(rwl, i % ReaderToWriterDivisor == 0);
        });
    }

    // Wait for the threads to be finished
    for (auto& thread : threads) {
      thread.join();
    }

    // Check intersections in results
    for (size_t i = 0; i < ThreadCount; ++i) {
      ASSERT_FALSE(results[i].isDefaulted()) << "Thread " << i << " did not assign a result value";

      // Find (first) intersecting result in [i + 1, ThreadCount)
      std::optional<size_t> iIntersecting;
      for (size_t j = i + 1; j < ThreadCount && !iIntersecting; ++j) {
        if (results[i].ran().intersectsWith(results[j].ran())) {
          iIntersecting = j;
        }
      }

      // Deal with intersecting results if they were found
      if (iIntersecting) {
        if (results[i].wrote() || results[*iIntersecting].wrote()) {
          auto threadType = [&results](size_t i) { return results[i].wrote() ? "writer" : "reader"; };
          FAIL() << "Intersection found between " << threadType(i) << " thread " << i
            << " and " << threadType(*iIntersecting) << "thread " << *iIntersecting;
        }
        else {
          intersectingReadFound = true;
        }
      }
    }
  }

  // We'd expect to find at least one intersecting read, given the values of our IntersectingReadAttempts and ThreadCount and ReaderToWriterRatio
  constexpr auto Writers = ThreadCount / ReaderToWriterDivisor
    + ((ThreadCount % ReaderToWriterDivisor == 0) ? 0 : 1); // Modulo bias favors writers
  constexpr auto Readers = ThreadCount - Writers;

  EXPECT_TRUE(intersectingReadFound) << "Reader/writer lock seems to interleave readers: no intersecting read found"
    << " after " << IntersectingReadAttempts << " attempts, each of which"
    << " used " << Readers << " reader threads"
    << " and " << Writers << " writer threads";
}

TEST(ReaderWriterLock, Recursive) {
  auto value = 0U;

  // (Plain, non-recursive) ReaderWriterLock supports recursive reads, giving us a new shared_lock every time
  pep::ReaderWriterLock nonrecursive;
  EXPECT_EQ(0U, nonrecursive.read([&]() {
    return nonrecursive.read([&]() {
      return value;
      });
    }));

  // ReaderWriterThreadLock supports recursive writes, in which case (plain, non-recursive) ReaderWriterLock would have deadlocked. Trust me, I've tested.
  pep::ReaderWriterThreadLock rwtl;
  EXPECT_NO_FATAL_FAILURE(
    rwtl.write([&]() {
      rwtl.write([&]() {
        EXPECT_EQ(value, 0U);
        ++value;
        });
      })
  );
  EXPECT_EQ(value, 1U);


  rwtl.write([&]() {
    rwtl.read([&]() {
      ++value; // OK to do inside a reader lock because we're also holding an (outer) writer lock
      });
    });
  EXPECT_EQ(value, 2U);

  EXPECT_THROW(
    rwtl.read([&]() {
      rwtl.write([&]() {
        ++value;
        });
      })
    , std::exception
  );
  EXPECT_EQ(value, 2U) << "Exception should have prevented update";

  rwtl.write([&]() { // Since we're holding an outer writer lock...
    rwtl.read([&]() { // ... we don't actually acquire a reader lock here...
      rwtl.write([&]() { // ... so now it's OK to invoke "write" inside a "read"
        ++value;
        });
      });
    });
  EXPECT_EQ(value, 3U);
}
