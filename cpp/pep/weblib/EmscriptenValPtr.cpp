#include <pep/weblib/EmscriptenValPtr.hpp>

#include <pep/utils/Log.hpp>

#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>

#include <utility>

using namespace pep;

namespace {
const std::string LogTag("EmscriptenValPtr");

void DeleteOnMainThread(emscripten::val* valPtr) noexcept {
  auto success = emscripten::ProxyingQueue{}.proxyAsync(emscripten_main_runtime_thread_id(), [valPtr] { delete valPtr; });
  if (!success) {
    PEP_LOG(LogTag, Severity::Warning) << "Failed to proxy val deletion to main thread, object will leak";
  }
}
}

pep::weblib::EmscriptenValPtr::EmscriptenValPtr(emscripten::val val)
    : valPtr_(std::shared_ptr<emscripten::val>(new emscripten::val(std::move(val)), DeleteOnMainThread)) {}
