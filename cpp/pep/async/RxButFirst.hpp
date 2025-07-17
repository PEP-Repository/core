#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-tap.hpp>

#include <cassert>
#include <functional>

namespace pep
{

// Returns an observable that imitates the given observable,
// but makes sure to call the given function exactly once
// _before_ sending any on_next, on_error, or on_complete to subscribers,
// but _after_ the source observable emitted an on_next, on_error 
// or on_complete.
template <typename Action>
struct RxButFirst {

  // check that Action can be invoked, and returns nothing
  static_assert(std::is_same_v<std::invoke_result_t<Action>, void>);

private:
  struct State {
    State(Action&& action) 
      : action(std::move(action)) {}

    void tryIt() {
      if (this->didIt)
        return;
      this->didIt = true;
      this->action();
    }

    void use() {
      assert(!this->used);
      this->used = true;
    }
  private:
    Action action;
    bool didIt = false;
    bool used = false;
  };

  std::shared_ptr<State> state;


public:
  explicit RxButFirst(Action&& doThis)
    : state(std::make_shared<State>(std::move(doThis))) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(
      rxcpp::observable<TItem, SourceOperator> obs) const {

    auto state = this->state;

    state->use();

    return obs.tap(
      // on next
      [state](const TItem&) {
        state->tryIt();
      },

      // on error
      [state](std::exception_ptr ep) {
        state->tryIt();
      },

      // on complete
      [state]() {
        state->tryIt();
      }
    );
  }
};

}

