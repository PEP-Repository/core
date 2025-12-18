#pragma once

#include <pep/async/OnEmscriptenThread.hpp>

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

namespace pep {

namespace detail {
struct WeblibApiPromiseAwaitTransform {
  // Well-known name
  template <typename TValue, typename TSourceOperator>
  auto await_transform(const rxcpp::observable<TValue, TSourceOperator>& observable) {
    // Return to previous thread after co_await
    return observable.observe_on(observe_on_emscripten_main_thread());
  }
};
}

/// Create coroutine returning Promise<any>.
/// Automatically switches back to starting thread before awaiting an observable
struct WeblibApiPromise : emscripten::val {
  using val::val;
  WeblibApiPromise(val v) : val(std::move(v)) {}

  // Well-known name
  struct promise_type : val::promise_type, detail::WeblibApiPromiseAwaitTransform {};
};

/// Create coroutine returning Promise<undefined>.
/// Automatically switches back to starting thread before awaiting an observable
struct WeblibApiVoidPromise : emscripten::val {
  using val::val;
  WeblibApiVoidPromise(val v) : val(std::move(v)) {}

  // Well-known name
  class promise_type : public detail::WeblibApiPromiseAwaitTransform {
    val::promise_type inner_;
  public:
    WeblibApiVoidPromise get_return_object() { return inner_.get_return_object(); }
    auto initial_suspend() noexcept { return inner_.initial_suspend(); }
    auto final_suspend() noexcept { return inner_.final_suspend(); }
    void unhandled_exception() { inner_.unhandled_exception(); }
    void reject_with(val error) { inner_.reject_with(std::move(error)); }

    void return_void() { inner_.return_value(undefined()); }
  };
};

}
