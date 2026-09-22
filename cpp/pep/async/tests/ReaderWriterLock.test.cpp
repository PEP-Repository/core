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
  bool overlapsWith(const TimeRange& other) const;
};

bool TimeRange::contains(pep::Timestamp when) const noexcept {
  return !this->isDefaulted() && when >= started_ && when <= ended_;
}

bool TimeRange::isContiguousWith(const TimeRange& other) const {
  return this->contains(other.started_) || this->contains(other.ended_);
}

bool TimeRange::overlapsWith(const TimeRange& other) const {
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
  static_assert(CHAR_BIT == 8);
  unsigned char randomized; // [0, 256)
  pep::RandomBytes({ &randomized, 1 });
  unsigned int msec = randomized % 4U; // [0, 64)
  return std::chrono::milliseconds(msec);
}

TimeRange Contention::SpendSomeTime() {
  auto duration = RandomDuration();
  auto started = pep::TimeNow();
  std::this_thread::sleep_for(duration);
  return TimeRange(started, pep::TimeNow());
}

Contention Contention::Run(pep::ReaderWriterLock& rwl, bool write) {
  std::function<TimeRange()> synchronized;
  if (write) {
    synchronized = [&rwl]() { return rwl.write(&SpendSomeTime); };
  } else {
    synchronized = [&rwl]() { return rwl.read(&SpendSomeTime); };
  }
  return Contention(write, synchronized());
}

}

TEST(ReaderWriterLock, Works) {
  constexpr size_t ThreadCount = 200;
  constexpr unsigned OverlappingReadAttempts = 5;
  constexpr unsigned ReaderToWriterRatio = 3; // One writer to this many readers: e.g. if set to 3, then 1 in 4 threads will be a writer (and 3 in 4 will be a reader)

  static_assert(ReaderToWriterRatio != 0, "Can't modulate very well otherwise, can I now?");
  constexpr unsigned ReaderToWriterDivisor = ReaderToWriterRatio + 1;

  bool overlapping_read_found = false;
  for (unsigned attempt = 0; (attempt < OverlappingReadAttempts) && !overlapping_read_found; ++attempt) {
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

    // Check overlaps in results
    for (size_t i = 0; i < ThreadCount; ++i) {
      ASSERT_FALSE(results[i].isDefaulted()) << "Thread " << i << " did not assign a result value";

      // Find (first) overlapping result in [i + 1, ThreadCount)
      std::optional<size_t> iOverlapping;
      for (size_t j = i + 1; j < ThreadCount && !iOverlapping; ++j) {
        if (results[i].ran().overlapsWith(results[j].ran())) {
          iOverlapping = j;
        }
      }

      // Deal with overlapping results if they were found
      if (iOverlapping) {
        if (results[i].wrote() || results[*iOverlapping].wrote()) {
          auto threadType = [&results](size_t i) { return results[i].wrote() ? "writer" : "reader"; };
          FAIL() << "Overlap between " << threadType(i) << " thread " << i
            << " and " << threadType(*iOverlapping) << "thread " << *iOverlapping;
        }
        else {
          overlapping_read_found = true;
        }
      }
    }
  }

  // We'd expect to find at least one overlapping read, given the values of our OverlappingReadAttempts and ThreadCount and ReaderToWriterRatio
  constexpr auto Writers = ThreadCount / ReaderToWriterDivisor
    + ((ThreadCount % ReaderToWriterDivisor == 0) ? 0 : 1); // Modulo bias favors writers
  constexpr auto Readers = ThreadCount - Writers;

  EXPECT_TRUE(overlapping_read_found) << "Reader/writer lock seems to interleave readers: no overlapping read found"
    << " after " << OverlappingReadAttempts << " attempts, each of which"
    << " used " << Readers << " reader threads"
    << " and " << Writers << " writer threads";
}
