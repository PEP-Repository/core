#include <pep/weblib/EmscriptenValPtr.hpp>

#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <utility>

using namespace pep;

namespace {
void DeleteOnMainThread(emscripten::val* valPtr) {
  emscripten::ProxyingQueue{}.proxyAsync(emscripten_main_runtime_thread_id(), [valPtr] { delete valPtr; });
}
}

EmscriptenValPtr::EmscriptenValPtr(emscripten::val val)
    : valPtr_(std::shared_ptr<emscripten::val>(new emscripten::val(std::move(val)), DeleteOnMainThread)) {}
