#pragma once

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace pep::weblib {

/// \brief Create batches that lazily read pages from a JS ReadableStream,
///        keeping at most one page in c++ memory at a time.
///        The rest of a chunk that spans several pages stays in JS, so that each byte is copied (into a page) only once.
/// \details The return type is \c pep::messaging::MessageBatches, spelled out because including
///          \c MessageSequence.hpp would pull protobuf and abseil into the web common library.
///          Must be called on the main thread.
/// \param reader A <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultReader">ReadableStreamDefaultReader</a>
///        producing <code>Uint8Array</code> chunks of any size, from a byte stream or a default stream,
///        e.g. from <code>Blob.stream().getReader()</code> or the output of a <code>CompressionStream</code>.
///        Will only be accessed on the main thread. The caller keeps ownership: it releases (or cancels) the reader.
/// \param pageSize The maximum number of bytes to emit at a time. The stream decides how much is read at a time:
///        chunks are split or combined into pages of this size
/// \param ioWorker Worker on which pages (and errors/completion) are delivered
/// \return Batches that together produce the entire contents of the stream
/// \remark Like \c IStreamToMessageBatches, no data is read until the (inner) batches are
///         subscribed to, and each batch must be exhausted before the next one is produced.
///         The result may only be subscribed to once, the read position is kept in shared state (and in the reader),
///         so a second subscription resumes where the first left off instead of restarting at the beginning of the stream.
rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
ReadableStreamToMessageBatches(emscripten::val reader, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker);

}
