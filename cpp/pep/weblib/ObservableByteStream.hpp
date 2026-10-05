#pragma once

#include <pep/weblib/Emscripten_fwd.hpp>

#include <rxcpp/rx-observable-fwd.hpp>

#include <cstddef>
#include <string>

namespace pep::weblib {

/// Create a ReadableStream that reuses buffers via \c autoAllocateChunkSize.
/// Items will be emitted on the same thread as the observable.
/// \param chunkSize The maximum size of each string in \p data.
/// \remark If \p chunkSize is too small for a string in \p data,
///   the implementation may need to allocate extra ArrayBuffers, instead of being able to reuse the same one,
///   which is less efficient.
/// \returns \c ReadableStream<Uint8Array<ArrayBuffer>>
emscripten::val CreateReadableByteStream(rxcpp::observable<std::string> data, std::size_t chunkSize);

}
