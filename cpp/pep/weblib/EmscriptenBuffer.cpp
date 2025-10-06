#include <pep/utils/CollectionUtils.hpp>
#include <pep/weblib/EmscriptenBuffer.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>

using namespace emscripten;
using namespace pep::weblib;

val Buffer::view() const {
  auto span = pep::ConvertBytes<std::uint8_t>(data);
  return val(typed_memory_view(span.size(), span.data()));
}

EMSCRIPTEN_BINDINGS(Buffer) {
  class_<Buffer>("Buffer")
      .function("view", &Buffer::view)
      ;
}
