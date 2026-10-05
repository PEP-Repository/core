#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <memory>
#include <boost/preprocessor/cat.hpp>

// golang-like defer based on
//   https://www.gingerbill.org/article/defer-in-cpp.html

namespace pep {

template <typename F>
struct Deferred {
  static_assert(!pep::DerivedFromSpecialization<F, Deferred>);
  Deferred(F&& f) : f_(std::move(f)) {}
  Deferred(const Deferred<F> &) = delete;
  Deferred(Deferred<F> &&) = delete;
  ~Deferred() {
    this->trigger();
  }
  void trigger() {
    if (triggered_)
      return;
    triggered_ = true;
    f_();
  }
private:
  F f_;
  bool triggered_ = false;
};

template <typename F>
Deferred<F> DeferFunc(F&& f) {
  return Deferred<F>(std::forward<F>(f));
}

#if defined(__clang__) && __clang_major__ >= 22
// For Clang >=22: Silence warning about __COUNTER__, which now apparently is a C2y extension
# define PEP_SILENCE_COUNTER_EXTENSION_WARNING_BEGIN \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Wc2y-extensions\"")
# define PEP_SILENCE_COUNTER_EXTENSION_WARNING_END \
  _Pragma("clang diagnostic pop")
#else
# define PEP_SILENCE_COUNTER_EXTENSION_WARNING_BEGIN
# define PEP_SILENCE_COUNTER_EXTENSION_WARNING_END
#endif

// The invocation of BOOST_PP_CAT(_defer_, __COUNTER__) produces unique tokens such as "_defer_1234".
// This macro defines a variable with that name, and its destructor will run the specified code at scope end.
#define PEP_DEFER(code) \
  auto PEP_SILENCE_COUNTER_EXTENSION_WARNING_BEGIN BOOST_PP_CAT(_defer_, __COUNTER__) PEP_SILENCE_COUNTER_EXTENSION_WARNING_END = \
    ::pep::DeferFunc([&](){code;})

// That an explicit lambda must be passed to DeferUnique is intentional,
// so that the programmer can control -and is aware of- the captures.
// (Beware of capturing unique pointers:  they make the lambda non-copyable,
// which causes problems for rxcpp.)
template<typename F>
std::unique_ptr<Deferred<F>> DeferUnique(F&& f) {
  return std::make_unique<Deferred<F>>(std::forward<F>(f));
}

template<typename F>
std::shared_ptr<Deferred<F>> DeferShared(F&& f) {
  return std::make_shared<Deferred<F>>(std::forward<F>(f));
}

}
