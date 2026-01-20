#pragma once

#include <sys/types.h>

#include <ostream>

namespace pep::weblib {

class CurrentThreadPrintable {
public:
  friend std::ostream& operator<<(std::ostream& os, const CurrentThreadPrintable&);
};

class ThreadPrintable {
  ::pthread_t thread_;
public:
  ThreadPrintable(::pthread_t thread) noexcept : thread_{thread} {}

  friend std::ostream& operator<<(std::ostream& os, const ThreadPrintable& self);
};

}
