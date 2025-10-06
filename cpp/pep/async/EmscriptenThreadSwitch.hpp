#pragma once

#include <coroutine>

#include <emscripten/proxying.h>

namespace pep {

/// Usage:
/// \code
///   EmscriptenThreadSwitch switchBack;
///   co_await thingThatSwitchesToOtherThread;
///   co_await switchBack;
/// \endcode
class EmscriptenThreadSwitch {
  pthread_t thread_;
  std::coroutine_handle<> coroutine_;
  emscripten::ProxyingQueue queue_;

public:
  /// Switch back to current thread
  EmscriptenThreadSwitch() noexcept;
  explicit EmscriptenThreadSwitch(pthread_t thread) noexcept : thread_(thread) {}

  // Well-known name
  [[nodiscard]] bool await_ready() const noexcept;

  // Well-known name
  void await_suspend(std::coroutine_handle<> h) noexcept;

  // Well-known name
  void await_resume() noexcept {}
};

}
