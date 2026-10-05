#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace pep::weblib {

/// Non-owning handle to cancel timeout
class SetTimeoutHandle {
  friend SetTimeoutHandle SetTimeout(std::chrono::duration<double, std::milli> delay, std::function<void()> callback);

  std::weak_ptr<struct SetTimeoutContext> ctx_;

  explicit SetTimeoutHandle(std::weak_ptr<SetTimeoutContext> ctx)
    : ctx_(std::move(ctx)) {}

public:
  /// Returns true if the timer can still be canceled, i.e. it has not yet executed or been canceled.
  [[nodiscard]] bool cancelable() const {
    return !ctx_.expired();
  }

  /// Cancel the timer. Does nothing if the timer has already executed or been canceled.
  /// Must be executed on the same thread that scheduled the timer.
  void cancel() const;
};

/// Execute \p callback after \p delay.
SetTimeoutHandle SetTimeout(std::chrono::duration<double, std::milli> delay, std::function<void()> callback);

}
