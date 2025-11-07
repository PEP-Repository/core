#pragma once

#include <chrono>

namespace pep::testing {

// (Properties of) the clock type used for timing-based tests
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

inline std::chrono::milliseconds MillisecondsSince(TimePoint time_point) {
  return duration_cast<std::chrono::milliseconds>(Clock::now() - time_point);
}

}
