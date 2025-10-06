#include <pep/weblib/WeblibApiPromise.hpp>

#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(MainThreadObservablePromise) {
  using namespace emscripten;
  register_type<pep::WeblibApiPromise>("Promise<any>");
  register_type<pep::WeblibApiVoidPromise>("Promise<void>");
}
