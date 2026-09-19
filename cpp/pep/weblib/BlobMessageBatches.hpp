#pragma once

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace pep::weblib {

/// \brief Create batches that lazily read page-sized chunks from a JS Blob (or File),
///        keeping at most one page in c++ memory at a time.
/// \details The return type is \c pep::messaging::MessageBatches, spelled out because including
///          \c MessageSequence.hpp would pull protobuf and abseil into the web common library.
///          Must be called on the main thread.
/// \param blob A <a href="https://developer.mozilla.org/en-US/docs/Web/API/Blob">Blob</a>,
///        e.g. a File obtained from an <code>&lt;input type="file"&gt;</code> element.
///        Will only be accessed on the main thread.
/// \param pageSize The maximum number of bytes to read (and emit) at a time
/// \param ioWorker Worker on which pages (and errors/completion) are delivered
/// \return Batches that together produce the entire contents of \p blob
/// \remark Like \c IStreamToMessageBatches, no data is read until the (inner) batches are
///         subscribed to, and each batch must be exhausted before the next one is produced.
///         The result may only be subscribed to once, the read position is kept in shared state,
///         so a second subscription resumes where the first left off instead of restarting at the beginning of the Blob.
rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>
BlobToMessageBatches(emscripten::val blob, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker);

}
