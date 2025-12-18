#pragma once

#include <rxcpp/rx-observable-fwd.hpp>

#include <cstddef>
#include <string>

// Forward declaration
namespace emscripten { class val; }

namespace pep {

/// Create a ReadableStream that reuses buffers via \c autoAllocateChunkSize.
/// Items will be emitted on the same thread as the observable.
/// \param chunkSize The maximum size of each string in \p data. If this is too small, additional buffers will need to be allocated.
/// \returns \c ReadableStream<Uint8Array<ArrayBuffer>>
emscripten::val CreateReadableByteStream(rxcpp::observable<std::string> data, std::size_t chunkSize);

}
