#include <pep/weblib/WeblibApiPromise.hpp>

#include <emscripten/bind.h>
#include <emscripten/threading.h>

#include <cassert>

namespace {
void PromiseConstructorAssertions(const emscripten::val& jsPromise) {
  // Our await_transform calls uses observe_on_emscripten_main_thread to return to the original thread,
  // which means that that original thread must be the main thread
  assert(::emscripten_is_main_runtime_thread() && "WeblibApiPromise must be used on main thread");
  assert(jsPromise.instanceof(emscripten::val::global("Promise")) && "jsPromise must be a Promise");
}
}

pep::weblib::WeblibApiPromise::WeblibApiPromise(val jsPromise) : val(std::move(jsPromise)) {
  PromiseConstructorAssertions(*this);
}
pep::weblib::WeblibApiVoidPromise::WeblibApiVoidPromise(val jsPromise) : val(std::move(jsPromise)) {
  PromiseConstructorAssertions(*this);
}

EMSCRIPTEN_BINDINGS(MainThreadObservablePromise) {
  using namespace emscripten;
  register_type<pep::weblib::WeblibApiPromise>("Promise<any>");
  register_type<pep::weblib::WeblibApiVoidPromise>("Promise<void>");
}
