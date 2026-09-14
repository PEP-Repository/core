#include <pep/async/ReaderWriterLock.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Timestamp.hpp>
#include <gtest/gtest.h>
#include <array>

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
  static TimeRange SpendTime();

public:
  Contention() noexcept = default;

  bool wrote() const noexcept { return wrote_; }
  TimeRange ran() const noexcept { return ran_; }
  bool isDefaulted() const noexcept { return ran_.isDefaulted(); }

  static Contention Run(pep::ReaderWriterLock& rwl, bool write);
};

std::chrono::milliseconds Contention::RandomDuration() {
  static_assert(CHAR_BIT == 8);
  unsigned char randomized;
  pep::RandomBytes({ &randomized, 1 });
  unsigned int msec = randomized % 4; // [0, 64) msec
  return std::chrono::milliseconds(msec);
}

TimeRange Contention::SpendTime() {
  auto duration = RandomDuration();
  auto started = pep::TimeNow();
  std::this_thread::sleep_for(duration);
  return TimeRange(started, pep::TimeNow());
}

Contention Contention::Run(pep::ReaderWriterLock& rwl, bool write) {
  std::function<TimeRange()> synchronized;
  if (write) {
    synchronized = [&rwl]() { return rwl.write(&SpendTime); };
  } else {
    synchronized = [&rwl]() { return rwl.read(&SpendTime); };
  }
  return Contention(write, synchronized());
}

}

TEST(ReaderWriterLock, Works) {
  constexpr size_t NOF_THREADS = 2000;

  std::array<std::thread, NOF_THREADS> threads;
  std::array<Contention, NOF_THREADS> results;
  pep::ReaderWriterLock rwl;

  for (size_t i = 0; i < NOF_THREADS; ++i) {
    threads[i] = std::thread([i, &rwl, &results]() {
        assert(results[i].isDefaulted());
        results[i] = Contention::Run(rwl, i % 2 == 0);
      });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  bool has_overlapping_read = false;
  for (size_t i = 0; i < NOF_THREADS; ++i) {
    ASSERT_FALSE(results[i].isDefaulted()) << "Thread " << i << " did not assign a result value";
    if (results[i].wrote()) {
      for (size_t j = i + 1U; j < NOF_THREADS; ++j) {
        EXPECT_FALSE(results[i].ran().overlapsWith(results[j].ran())) << "Writer thread " << i << " overlapped with " << (results[j].wrote() ? "writer" : "reader") << " thread " << j;
      }
    }
    else {
      for (size_t j = i + 1U; j < NOF_THREADS; ++j) {
        auto overlap = results[i].ran().overlapsWith(results[j].ran());
        if (overlap) {
          if (results[j].wrote()) {
            FAIL() << "Reader thread " << i << " overlapped with writer thread " << j;
          }
          else {
            has_overlapping_read = true;
          }
        }
      }
    }
  }

  EXPECT_TRUE(has_overlapping_read) << "No overlapping read detected: reader threads seem to have been interleaved"; // TODO: retry a coupla times
}
