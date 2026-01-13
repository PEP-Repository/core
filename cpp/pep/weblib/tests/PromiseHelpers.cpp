#include <pep/weblib/tests/PromiseHelpers.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>

#include <emscripten/bind.h>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-subscribe_on.hpp>

#include <stdexcept>

using namespace emscripten;
using namespace pep;

namespace {

const std::string LOG_TAG = "weblib tests PromiseHelpers";

//NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) We do not run tests concurrently
rxcpp::observer<std::string> CurrentTest;

void ResolveTest(std::string result) {
  LOG(LOG_TAG, debug) << "ResolveTest: " << result;
  CurrentTest.on_next(std::move(result));
  CurrentTest.on_completed();
}

void RejectTest(const val& error) {
  auto str = val::global("String")(error).as<std::string>();
  LOG(LOG_TAG, debug) << "RejectTest: " << str;
  if (error.instanceof(val::global("WebAssembly")["Exception"])) {
    try {
      error.throw_();
    } catch (...) {
      LOG(LOG_TAG, debug) << "RejectTest caught C++ exception: " << GetExceptionMessage(std::current_exception());
      CurrentTest.on_error(std::current_exception());
    }
  } else {
    // JS exceptions cannot be caught from C++: https://github.com/emscripten-core/emscripten/issues/11496
    CurrentTest.on_error(std::make_exception_ptr(std::runtime_error(str)));
  }
}

EMSCRIPTEN_BINDINGS(reportTest) {
  function("pepResolveTest", &ResolveTest);
  function("pepRejectTest", &RejectTest);
}

}

// Would be nice if val::awaiter supported non-Promise coroutines, but it doesn't,
//  see https://github.com/emscripten-core/emscripten/issues/26064
std::string pep::weblib::tests::PromiseTest(std::function<val()> createPromise) {
  return CreateObservable<std::string>(
          [createPromise = std::move(createPromise)](rxcpp::subscriber<std::string> subscriber) {
            CurrentTest = subscriber.get_observer();
            createPromise().call<void>("then",
              val::module_property("pepResolveTest"),
              val::module_property("pepRejectTest"));
            LOG(LOG_TAG, verbose) << "Promise created";
          })
      // Because of -sPROXY_TO_PTHREAD, we are not the main thread.
      // We schedule this on the other (actual main) thread, so that it can complete there while we block here.
      .subscribe_on(observe_on_emscripten_main_thread())
      // IDK why, but it hangs without this or a no-op .tap() before as_blocking...
      .op(RxGetOne())
      .as_blocking().last();
}
