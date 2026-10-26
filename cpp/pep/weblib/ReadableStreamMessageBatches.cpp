#include <pep/weblib/ReadableStreamMessageBatches.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Log.hpp>
#include <pep/weblib/EmscriptenValPtr.hpp>
#include <pep/weblib/OnEmscriptenThread.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <rxcpp/operators/rx-subscribe_on.hpp>

#include <boost/noncopyable.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>

using namespace emscripten;
using namespace pep;

namespace {

const std::string LogTag("ReadableStreamMessageBatches");

/// Same types as \c pep::messaging::MessageSequence and \c pep::messaging::MessageBatches
using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;
using MessageBatches = rxcpp::observable<MessageSequence>;

/// Shared state for reading consecutive pages from a JS ReadableStream.
/// Reads happen on the main thread, pages are delivered on \c ioWorker.
struct StreamReadState {
  weblib::EmscriptenValPtr reader; ///< ReadableStreamDefaultReader. Only access on the main thread
  std::size_t pageSize; ///< The maximum number of bytes to emit at a time
  rxcpp::observe_on_one_worker ioWorker; ///< Worker on which pages are delivered
  /// The chunk being copied into pages: chunks do not line up with pages, so a chunk may span several pages.
  /// The part not copied yet stays in JS, so that each byte is copied only once. Only access on the main thread
  std::optional<weblib::EmscriptenValPtr> chunk;
  std::size_t chunkOffset = 0; ///< Bytes of \c chunk already copied into pages. Only access on the main thread
  /// The reader reported done: no more chunks, so the page being filled is the last one.
  /// Written on the main thread, read there and on the io worker, but never concurrently:
  /// the io worker only reads it once the batch is exhausted, which is separated by a queue hop.
  std::atomic<bool> streamDone = false;
  rxcpp::observe_on_one_worker mainWorker = weblib::observe_on_emscripten_main_thread();
};

/// Fills a page from chunks of a ReadableStream, reading chunks as needed, and passes it to the inner subscriber.
/// Runs on the main thread, where the reader handle is valid, and deletes itself once the page is emitted
class StreamPageReader : public boost::noncopyable {
  std::shared_ptr<StreamReadState> state_;
  rxcpp::subscriber<std::shared_ptr<std::string>> inner_;
  val self_;
  /// The page being filled. Allocated at its full size, as it is filled in several (asynchronous) steps
  std::shared_ptr<std::string> page_;
  std::size_t filled_ = 0; ///< Bytes of \c page_ filled so far

  StreamPageReader(std::shared_ptr<StreamReadState> state, rxcpp::subscriber<std::shared_ptr<std::string>> inner)
    : state_(std::move(state)), inner_(std::move(inner)), page_(std::make_shared<std::string>(state_->pageSize, '\0')) {}

  void deleteSelf() {
    self_.call<void>("delete");
    PEP_LOG(LogTag, Severity::Verbose) << this << " deleted self";
  }

  /// Copies from the current chunk into the page until the page is full or the stream is done,
  /// reading the next chunk when the current one is used up (asynchronously, continuing in \c onChunk)
  void next() {
    auto& state = *state_;
    while (filled_ < state.pageSize) {
      if (!state.chunk) {
        if (state.streamDone) {
          break;
        }
        PEP_LOG(LogTag, Severity::Verbose) << this << " reading chunk, " << filled_ << " bytes of page filled";
        val promise = state.reader->call<val>("read");
        promise.call<val>("then", self_["onChunk"].call<val>("bind", self_), self_["onError"].call<val>("bind", self_));
        return;
      }
      const val& chunk = **state.chunk;
      const auto chunkLength = chunk["byteLength"].as<std::size_t>();
      const auto length = std::min(state.pageSize - filled_, chunkLength - state.chunkOffset);
      // The only copy: straight from (a view on) the JS chunk into its place in the page
      const std::span bytes = ConvertBytes<std::uint8_t>(std::span{page_->data() + filled_, length});
      val(typed_memory_view(bytes.size(), bytes.data()))
          .call<void>("set", chunk.call<val>("subarray", state.chunkOffset, state.chunkOffset + length));
      filled_ += length;
      state.chunkOffset += length;
      if (state.chunkOffset == chunkLength) {
        // Used up (or empty): read the next chunk when more data is needed
        state.chunk.reset();
        state.chunkOffset = 0;
      }
    }
    emitPage();
  }

  /// Emits the page, unless it is empty, and completes the inner subscriber
  void emitPage() {
    // An empty stream produces a single empty batch, mirroring IStreamToMessageBatches on an empty stream
    if (filled_ != 0) {
      page_->resize(filled_); // Only shrinks (the last page), so no copy
      inner_.on_next(std::move(page_));
    }
    inner_.on_completed();
    deleteSelf();
  }

public:
  /// \brief Produces a \c MessageSequence containing a single page from the stream.
  /// \remark Postpones reading data from the stream until someone .subscribe()s to the \c MessageSequence
  static MessageSequence ReadPage(std::shared_ptr<StreamReadState> state) {
    return CreateObservable<std::shared_ptr<std::string>>(
        [state](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
          //TODO(workaround) "new" is a workaround to not copy, see https://github.com/emscripten-core/emscripten/issues/25412
          val self(new StreamPageReader(state, std::move(inner)), allow_raw_pointers{});
          auto* reader = self.as<StreamPageReader*>(allow_raw_pointers{});
          reader->self_ = self;
          reader->next();
        })
        // The reader may only be accessed on the main thread, so subscribe (and therefore read) there
        .subscribe_on(state->mainWorker);
  }

  /// Handles the result of \c reader.read(): <code>{done, value}</code>
  void onChunk(val result) {
    try {
      if (result["done"].as<bool>()) {
        state_->streamDone = true;
      } else {
        val chunk = result["value"];
        if (!chunk.instanceof(val::global("Uint8Array"))) {
          throw std::invalid_argument("ReadableStream must produce Uint8Array chunks");
        }
        state_->chunk = weblib::EmscriptenValPtr(std::move(chunk));
      }
      next();
    } catch (...) {
      inner_.on_error(std::current_exception());
      deleteSelf();
    }
  }

  void onError(val error) {
    auto message = val::global("String")(std::move(error)).as<std::string>();
    PEP_LOG(LogTag, Severity::Debug) << this << " ReadableStream read failed: " << message;
    inner_.on_error(std::make_exception_ptr(std::runtime_error("Failed to read from ReadableStream: " + message)));
    deleteSelf();
  }
};

/// Produces a single page as a \c MessageSequence ("batch"), delivered on the io worker.
MessageSequence MakeStreamBatch(std::shared_ptr<StreamReadState> state) {
  return StreamPageReader::ReadPage(state)
      // Deliver the page (and errors/completion) back on the io worker
      .observe_on(state->ioWorker);
}

/// Counterpart of \c ProvideBatch in \c MessageSequence.cpp, reading from a
/// ReadableStream instead of an \c std::istream. Runs on the io worker.
void ProvideStreamBatch(std::shared_ptr<StreamReadState> state, rxcpp::subscriber<MessageSequence> outer) {
  if (!state->streamDone) {
    outer.on_next(MakeStreamBatch(state) // Provide a single page as a MessageSequence
        .op(RxSubsequently([state, outer]() { // that must be exhausted before
          ProvideStreamBatch(state, outer); // continuing with the next
        })));
  } else {
    outer.on_completed();
  }
}

}

EMSCRIPTEN_BINDINGS(ReadableStreamMessageBatches) {
  class_<StreamPageReader>("StreamPageReader")
      .function("onChunk", &StreamPageReader::onChunk)
      .function("onError", &StreamPageReader::onError)
      ;
}

namespace pep::weblib {

MessageBatches ReadableStreamToMessageBatches(val reader, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker) {
  if (pageSize == 0) {
    throw std::invalid_argument("pageSize must be nonzero");
  }
  auto state = std::make_shared<StreamReadState>(std::move(reader), pageSize, std::move(ioWorker));
  return CreateObservable<MessageSequence>(
      [state](rxcpp::subscriber<MessageSequence> outer) {
        ProvideStreamBatch(state, outer);
      });
}

}
