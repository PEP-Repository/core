#pragma once

#include <exception>
#include <coroutine>
#include <optional>
#include <utility>
#include <variant>

#include <boost/noncopyable.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

/// Await an observable returning a single item inside a coroutine, returns when the observable completes or errors.
/// The coroutine will be resumed on the observing thread, which can be different from the current thread.
/// Awaiter throws std::runtime_error If the observable does not contain exactly 1 item.
template<typename TValue, typename TSourceOperator>
[[nodiscard]] auto operator co_await(rxcpp::observable<TValue, TSourceOperator> obs) {

  class ValueObservableAwaiter : boost::noncopyable {
    // Wrapper in case TValue == std::exception_ptr
    struct Error {
      std::exception_ptr ex;
    };

    using ResultUnion = std::variant<TValue, Error>;

    struct New {
      rxcpp::observable<TValue, TSourceOperator> observable;
    };

    struct Running {
      std::coroutine_handle<> coroutine;
      // Result or internal error
      std::optional<ResultUnion> result;
    };

    struct Completed {
      // Result or (internal) error
      ResultUnion result;
    };

    std::variant<New, Running, Completed> state_;

  public:
    explicit ValueObservableAwaiter(rxcpp::observable<TValue, TSourceOperator> obs)
      : state_(New{std::move(obs)}) {}

    // Well-known name
    [[nodiscard]] constexpr bool await_ready() const noexcept { return std::holds_alternative<Completed>(state_); }

    // Well-known name
    void await_suspend(std::coroutine_handle<> h) {
      auto obs = std::move(std::get<New>(state_)).observable;
      state_.template emplace<Running>(h);
      obs.subscribe(
        // on next (could be called multiple times)
        [this](TValue val) noexcept {
          // Assumes we are not Completed yet
          if (auto& result = std::get<Running>(state_).result) {
            result->template emplace<Error>(
              std::make_exception_ptr(std::runtime_error("Multiple results produced in ValueObservableAwaiter")));
          } else {
            result.emplace(std::move(val));
          }
        },
        // on error
        [this](std::exception_ptr ex) noexcept {
          // Assumes we are not Completed yet
          auto&& runningState = std::get<Running>(state_);
          auto coro = std::move(runningState.coroutine);
          state_.template emplace<Completed>(Error{std::move(ex)});
          coro.resume();
        },
        // on completed (not called on error)
        [this]() noexcept {
          // Assumes we are not Completed yet
          auto&& runningState = std::get<Running>(state_);
          ResultUnion result;
          if (runningState.result) {
            result = std::move(*runningState.result);
          } else {
            result.template emplace<Error>(
              std::make_exception_ptr(std::runtime_error("No results produced in ValueObservableAwaiter")));
          }

          auto coro = std::move(runningState.coroutine);
          state_.template emplace<Completed>(std::move(result));
          coro.resume();
        });
    }

    // Well-known name
    // Assumes we are Completed. Moves the value: should not be called twice
    TValue await_resume() {
      auto&& result = std::get<Completed>(state_).result;
      if (Error* err = std::get_if<Error>(&result)) {
        std::rethrow_exception(std::move(err->ex));
      }
      return std::move(std::get<TValue>(result));
    }
  };

  return ValueObservableAwaiter(std::move(obs));
}

} // namespace pep
