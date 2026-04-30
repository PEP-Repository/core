#include <pep/weblib/EmscriptenValPtr.hpp>

#include <pep/utils/Log.hpp>

#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <utility>

using namespace pep;

namespace {
const std::string LOG_TAG("EmscriptenValPtr");

void DeleteOnMainThread(emscripten::val* valPtr) noexcept {
  auto success = emscripten::ProxyingQueue{}.proxyAsync(emscripten_main_runtime_thread_id(), [valPtr] { delete valPtr; });
  if (!success) {
    LOG(LOG_TAG, warning) << "Failed to proxy val deletion to main thread, object will leak";
  }
}
}

pep::weblib::EmscriptenValPtr::EmscriptenValPtr(emscripten::val val)
    : valPtr_(std::shared_ptr<emscripten::val>(new emscripten::val(std::move(val)), DeleteOnMainThread)) {}
