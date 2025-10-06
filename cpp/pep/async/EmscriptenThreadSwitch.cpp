#include <pep/async/EmscriptenThreadSwitch.hpp>

#include <stdexcept>
#include <utility>

#include <pthread.h>

using namespace pep;

EmscriptenThreadSwitch::EmscriptenThreadSwitch() noexcept : thread_(pthread_self()) {}

bool EmscriptenThreadSwitch::await_ready() const noexcept { return pthread_equal(pthread_self(), thread_); }

void EmscriptenThreadSwitch::await_suspend(std::coroutine_handle<> h) noexcept {
  coroutine_ = h;
  bool success = queue_.proxyAsync(thread_, [this] {
    coroutine_.resume();
  });
  if (!success) {
    try {
      // Terminate with message.
      // I made this function noexcept because not switching threads is usually not an option.
      throw std::runtime_error("Failed to switch threads");
    } catch (...) {
      std::terminate();
    }
  }
}
