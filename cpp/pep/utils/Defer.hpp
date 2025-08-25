#pragma once

#include <memory>
#include <boost/preprocessor/cat.hpp>

// golang-like defer based on
//   https://www.gingerbill.org/article/defer-in-cpp.html

template <typename F>
struct deferred {
  deferred(F&& f) : f(std::move(f)) {}
  deferred(const deferred<F> &) = delete;
  deferred(deferred<F> &&) = delete;
  ~deferred() { 
    this->trigger();
  }
  void trigger() {
    if (this->triggered)
      return;
    this->triggered = true; 
    this->f();
  }
private:
  F f;
  bool triggered = false;
};

template <typename F>
deferred<F> defer_func(F&& f) {
  return deferred<F>(std::forward<F>(f));
}

// The invocation of BOOST_PP_CAT(_defer_, __COUNTER__) produces unique tokens such as "_defer_1234".
// This macro defines a variable with that name, and its destructor will run the specified code at scope end.
#define PEP_DEFER(code) auto BOOST_PP_CAT(_defer_, __COUNTER__) = defer_func([&](){code;})

// That an explicit lambda must be passed to defer_unique is intentional,
// so that the programmer can control -and is aware of- the captures.
// (Beware of capturing unique pointers:  they make the lambda non-copyable,
// which causes problems for rxcpp.)
template<typename F>
std::unique_ptr<deferred<F>> defer_unique(F&& f) {
  return std::make_unique<deferred<F>>(std::forward<F>(f));
}

template<typename F>
std::shared_ptr<deferred<F>> defer_shared(F&& f) {
  return std::make_shared<deferred<F>>(std::forward<F>(f));
}
