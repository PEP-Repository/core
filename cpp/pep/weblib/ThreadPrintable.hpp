#pragma once

#include <sys/types.h>

#include <ostream>

namespace pep::weblib {

class ThreadPrintable {
  ::pthread_t thread_;
public:
  ThreadPrintable(::pthread_t thread) noexcept : thread_{thread} {}
  /// ThreadPrintable for current thread
  ThreadPrintable() noexcept;

  friend std::ostream& operator<<(std::ostream& os, const ThreadPrintable& self);
};

}
