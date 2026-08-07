#pragma once

#include <pep/weblib/OnEmscriptenThread.hpp>

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

namespace pep::weblib {

namespace detail {
struct WeblibApiPromiseAwaitTransform {
  // Well-known name
  template <typename TValue, typename TSourceOperator>
  auto await_transform(const rxcpp::observable<TValue, TSourceOperator>& observable) {
    // Return to main thread after co_await
    return observable.observe_on(observe_on_emscripten_main_thread());
  }
};
}

/// Create coroutine returning Promise<any>.
/// Automatically switches back to main thread before awaiting an observable.
/// Must be used on the main thread.
struct WeblibApiPromise : emscripten::val {
  explicit WeblibApiPromise(val jsPromise);

  // Well-known name
  struct promise_type : val::promise_type, detail::WeblibApiPromiseAwaitTransform {
    // Well-known name
    WeblibApiPromise get_return_object() { //NOLINT(bugprone-derived-method-shadowing-base-method)
      return WeblibApiPromise(val::promise_type::get_return_object());
    }
  };
};

/// Create coroutine returning Promise<undefined>.
/// Automatically switches back to main thread before awaiting an observable.
/// Must be used on the main thread.
struct WeblibApiVoidPromise : emscripten::val {
  explicit WeblibApiVoidPromise(val jsPromise);

  // Well-known name
  class promise_type : public detail::WeblibApiPromiseAwaitTransform {
    // We cannot inherit from val::promise_type here,
    // because we want to expose return_void instead of return_value.
    // Deleting return_value does not work, it must not be declared at all.
    val::promise_type inner_;
  public:
    // Well-known name
    WeblibApiVoidPromise get_return_object() { return WeblibApiVoidPromise(inner_.get_return_object()); }
    // Well-known name
    auto initial_suspend() noexcept { return inner_.initial_suspend(); }
    // Well-known name
    auto final_suspend() noexcept { return inner_.final_suspend(); }
    // Well-known name
    void unhandled_exception() { inner_.unhandled_exception(); }

    // Well-known name
    void return_void() { inner_.return_value(undefined()); }
  };
};

}
