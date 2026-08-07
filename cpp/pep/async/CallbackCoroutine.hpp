#pragma once

#include <pep/async/FakeVoid.hpp>

#include <concepts>
#include <coroutine>
#include <optional>
#include <functional>
#include <utility>

namespace pep {

namespace callback_coroutine_detail {

template <typename T>
struct Callback { using type = void(T); };
template <>
struct Callback<void> { using type = void(); };

template <typename T>
class PromiseBase {
protected:
  std::function<typename Callback<T>::type> callback_;
  std::function<void()> errorCallback_; // noexcept

  std::optional<std::conditional_t<std::same_as<T, void>, FakeVoid, T>> value_;

public:
  PromiseBase(
      decltype(callback_) callback,
      decltype(errorCallback_) errorCallback,
      const auto& ...)
      : callback_(std::move(callback)),
        errorCallback_(std::move(errorCallback)) {}

  // Well-known name
  auto initial_suspend() const noexcept { return std::suspend_never{}; }
  // Well-known name
  auto final_suspend() const noexcept {
    if (value_) {
      try {
        // Execute callback in final_suspend, because in return_void local variables are still alive
        if constexpr (std::same_as<T, void>) {
          callback_();
        } else {
          callback_(std::move(*value_));
        }
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
      this->value_.emplace(std::forward<decltype(value)>(value));
    }
  };
};

template <>
class CallbackCoroutine<void> {
public:
  // Well-known name
  class promise_type : public callback_coroutine_detail::PromiseBase<void> {
  public:
    using callback_coroutine_detail::PromiseBase<void>::PromiseBase;

    // Well-known name
    CallbackCoroutine get_return_object() const noexcept { return CallbackCoroutine(); }

    // Well-known name
    void return_void() { value_.emplace(); }
  };
};

}
