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
class RxButFirst {
  // check that Action can be invoked, and returns nothing
  static_assert(std::is_same_v<std::invoke_result_t<Action>, void>);

  class State;
  std::shared_ptr<State> state_;

public:
  explicit RxButFirst(Action&& doThis)
    : state_(std::make_shared<State>(std::move(doThis))) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(
      rxcpp::observable<TItem, SourceOperator> obs) const {

    auto state = state_;
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

template <typename Action>
class RxButFirst<Action>::State {
private:
  Action action_;
  bool didIt_ = false;
  bool used_ = false;

public:
  State(Action&& action)
    : action_(std::move(action)) {}

  void tryIt() {
    if (didIt_)
      return;
    didIt_ = true;
    action_();
  }

  void use() {
    assert(!this->used_);
    used_ = true;
  }
};


}

