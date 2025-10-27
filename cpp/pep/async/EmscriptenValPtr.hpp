#pragma once

#include <emscripten/val.h>

#include <memory>

namespace pep {

/// emscripten::val smart-pointer wrapper that destructs value on the main thread
/// and is safe to copy on other threads.
class EmscriptenValPtr {
  std::shared_ptr<emscripten::val> valPtr_;

public:
  EmscriptenValPtr() = default;
  EmscriptenValPtr(emscripten::val val);

  EmscriptenValPtr(const EmscriptenValPtr &) = default;
  EmscriptenValPtr &operator=(const EmscriptenValPtr &) = default;
  EmscriptenValPtr(EmscriptenValPtr &&) = default;
  EmscriptenValPtr &operator=(EmscriptenValPtr &&) = default;

  emscripten::val& operator*() const { return *valPtr_; }
  emscripten::val* operator->() const { return valPtr_.get(); }
};

}
