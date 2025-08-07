#pragma once

#include <chrono>

namespace pep::testing {

// (Properties of) the clock type used for timing-based tests
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

/* To prevent gtest from reporting std::chrono::duration values as e.g. "8-byte-object <48-F1 AC-0F 00-00 00-00>",
 * we specify values in milliseconds, which we express as a fundamental signed integral type
 * (e.g. long long in my implementation).
 */
using Milliseconds = std::chrono::milliseconds::rep;

constexpr Duration MillisecondsToDuration(Milliseconds msec) {
  return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(msec));
}

inline pep::testing::Milliseconds MillisecondsSince(const TimePoint& time_point) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - time_point).count();
}

}
