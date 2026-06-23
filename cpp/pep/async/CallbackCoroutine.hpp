#pragma once

#include <pep/async/FakeVoid.hpp>

#include <concepts>
#include <coroutine>
#include <optional>
#include <functional>
#include <utility>

namespace pep {

namespace callback_coroutine_detail {

template <typename T, typename TValue = T>
class PromiseBase {
public:
  using Callback = std::function<void(TValue)>;
  using ErrorCallback = std::function<void()>; // noexcept

private:
  Callback callback_;
  ErrorCallback errorCallback_; // noexcept
  std::optional<TValue> value_;

protected:
  PromiseBase(
      Callback callback,
      ErrorCallback errorCallback,
      const auto& ...)
      : callback_(std::move(callback)),
        errorCallback_(std::move(errorCallback)) {}

  template <typename U = T>
  void storeValue(U&& value) {
    this->value_.emplace(std::forward<decltype(value)>(value));
  }

public:
  // Well-known name
  auto initial_suspend() const noexcept { return std::suspend_never{}; }
  // Well-known name
  auto final_suspend() const noexcept {
    if (value_) {
      try {
        // Execute callback in final_suspend, because in return_void local variables are still alive
        callback_(std::move(*value_));
      } catch (...) {
        errorCallback_();
      }
    }
    return std::suspend_never{};
  }

  // Well-known name
  void unhandled_exception() noexcept { errorCallback_(); }
};

}

template <typename T>
class CallbackCoroutine {
public:
  // Well-known name
  class promise_type : public callback_coroutine_detail::PromiseBase<T> {
  public:
    using callback_coroutine_detail::PromiseBase<T>::PromiseBase;

    // Well-known name
    CallbackCoroutine get_return_object() const noexcept { return CallbackCoroutine(); }

    // Well-known name
    template <typename U = T>
    void return_value(U&& value) {
      this->storeValue(std::forward<decltype(value)>(value));
    }
  };
};

template <>
class CallbackCoroutine<void> {
public:
  // Well-known name
  class promise_type : public callback_coroutine_detail::PromiseBase<void, FakeVoid> {
  public:
    using Callback = std::function<void()>;

    promise_type(Callback callback, ErrorCallback errorCallback, const auto& ...)
      : callback_coroutine_detail::PromiseBase<void, FakeVoid>([cb = std::move(callback)](FakeVoid) { cb(); }, std::move(errorCallback)) {}

    // Well-known name
    CallbackCoroutine get_return_object() const noexcept { return CallbackCoroutine(); }

    // Well-known name
    void return_void() { this->storeValue(FakeVoid()); }
  };
};

}
