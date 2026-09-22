#include <pep/weblib/BlobMessageBatches.hpp>

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
#include <span>
#include <stdexcept>

using namespace emscripten;
using namespace pep;

namespace {

const std::string LogTag("BlobMessageBatches");

/// Same types as \c pep::messaging::MessageSequence and \c pep::messaging::MessageBatches
using MessageSequence = rxcpp::observable<std::shared_ptr<std::string>>;
using MessageBatches = rxcpp::observable<MessageSequence>;

/// Shared state for reading consecutive pages from a JS Blob.
/// Reads happen on the main thread, pages are delivered on \c ioWorker.
struct BlobReadState {
  weblib::EmscriptenValPtr blob; ///< Only access on the main thread
  std::uint64_t size; ///< Blob size in bytes, captured at creation
  std::size_t pageSize; ///< The maximum number of bytes to read at a time
  rxcpp::observe_on_one_worker ioWorker; ///< Worker on which pages are delivered
  /// Bytes read so far. Written on the main thread, read on the io worker, but never concurrently:
  /// each access is separated by a queue hop, and the next read waits for the batch to complete.
  std::atomic<std::uint64_t> offset = 0;
  rxcpp::observe_on_one_worker mainWorker = weblib::observe_on_emscripten_main_thread();
};

/// Reads a single page from a Blob and passes it to the inner subscriber.
/// Runs on the main thread, where the Blob handle is valid, and deletes itself once the read is done
class BlobPageReader : public boost::noncopyable {
  std::shared_ptr<BlobReadState> state_;
  rxcpp::subscriber<std::shared_ptr<std::string>> inner_;
  val self_;

  BlobPageReader(std::shared_ptr<BlobReadState> state, rxcpp::subscriber<std::shared_ptr<std::string>> inner)
    : state_(std::move(state)), inner_(std::move(inner)) {}

  void deleteSelf() {
    self_.call<void>("delete");
    PEP_LOG(LogTag, Severity::Verbose) << this << " deleted self";
  }

  void start() {
    const auto offset = state_->offset.load();
    const auto end = std::min(offset + state_->pageSize, state_->size);
    PEP_LOG(LogTag, Severity::Verbose) << this << " reading Blob bytes [" << offset << ", " << end << ")";
    val promise = state_->blob->call<val>("slice", static_cast<double>(offset), static_cast<double>(end)).call<val>("arrayBuffer");
    promise.call<val>("then", self_["onPage"].call<val>("bind", self_), self_["onError"].call<val>("bind", self_));
  }

public:
  /// \brief Produces a \c MessageSequence containing a single page from the Blob.
  /// \remark Postpones reading data from the Blob until someone .subscribe()s to the \c MessageSequence
  static MessageSequence ReadPage(std::shared_ptr<BlobReadState> state) {
    return CreateObservable<std::shared_ptr<std::string>>(
        [state](rxcpp::subscriber<std::shared_ptr<std::string>> inner) {
          //TODO(workaround) "new" is a workaround to not copy, see https://github.com/emscripten-core/emscripten/issues/25412
          val self(new BlobPageReader(state, std::move(inner)), allow_raw_pointers{});
          auto* reader = self.as<BlobPageReader*>(allow_raw_pointers{});
          reader->self_ = self;
          reader->start();
        })
        // The Blob may only be accessed on the main thread, so subscribe (and therefore read) there
        .subscribe_on(state->mainWorker);
  }

  void onPage(val arrayBuffer) {
    try {
      const auto length = arrayBuffer["byteLength"].as<std::size_t>();
      if (length == 0) {
        throw std::runtime_error("Read no data from Blob (did the underlying file change?)");
      }
      // Let JS copy straight into the page. arrayBuffer.as<std::string>() would copy twice, via a temporary buffer.
      auto page = std::make_shared<std::string>();
      // resize_and_overwrite requires that its callback not throw, so a failure is captured and rethrown below
      std::exception_ptr failure;
      page->resize_and_overwrite(length, [&](char* buffer, std::size_t size) noexcept {
        try {
          const std::span bytes = ConvertBytes<std::uint8_t>(std::span{buffer, size});
          val(typed_memory_view(bytes.size(), bytes.data()))
              .call<void>("set", val::global("Uint8Array").new_(std::move(arrayBuffer)));
          return size;
        }
        catch (...) {
          failure = std::current_exception();
          return std::size_t{0}; // Discards the buffer, leaving no indeterminate bytes behind
        }
      });
      if (failure) {
        std::rethrow_exception(failure);
      }
      state_->offset += length;
      inner_.on_next(std::move(page));
      inner_.on_completed();
    } catch (...) {
      inner_.on_error(std::current_exception());
    }
    deleteSelf();
  }

  void onError(val error) {
    auto message = val::global("String")(std::move(error)).as<std::string>();
    PEP_LOG(LogTag, Severity::Debug) << this << " Blob read failed: " << message;
    inner_.on_error(std::make_exception_ptr(std::runtime_error("Failed to read from Blob: " + message)));
    deleteSelf();
  }
};

/// Produces a single page as a \c MessageSequence ("batch"), delivered on the io worker.
MessageSequence MakeBlobBatch(std::shared_ptr<BlobReadState> state) {
  return BlobPageReader::ReadPage(state)
      // Deliver the page (and errors/completion) back on the io worker
      .observe_on(state->ioWorker);
}

/// Counterpart of \c ProvideBatch in \c MessageSequence.cpp, reading from a
/// Blob instead of an \c std::istream. Runs on the io worker.
void ProvideBlobBatch(std::shared_ptr<BlobReadState> state, rxcpp::subscriber<MessageSequence> outer) {
  if (state->offset < state->size) {
    outer.on_next(MakeBlobBatch(state) // Provide a single page as a MessageSequence
        .op(RxSubsequently([state, outer]() { // that must be exhausted before
          ProvideBlobBatch(state, outer); // continuing with the next
        })));
  } else {
    outer.on_completed();
  }
}

}

EMSCRIPTEN_BINDINGS(BlobMessageBatches) {
  class_<BlobPageReader>("BlobPageReader")
      .function("onPage", &BlobPageReader::onPage)
      .function("onError", &BlobPageReader::onError)
      ;
}

namespace pep::weblib {

MessageBatches BlobToMessageBatches(val blob, std::size_t pageSize, rxcpp::observe_on_one_worker ioWorker) {
  if (pageSize == 0) {
    throw std::invalid_argument("pageSize must be nonzero");
  }
  // JS as a Number (double) holds integers exactly up to 2^53, so this is lossless for any Blob a browser can hand us.
  const auto size = static_cast<std::uint64_t>(blob["size"].as<double>());
  if (size == 0) {
    // Empty Blob: a single empty batch, mirroring IStreamToMessageBatches on an empty stream
    return rxcpp::observable<>::just(
        rxcpp::observable<>::empty<std::shared_ptr<std::string>>().as_dynamic());
  }
  auto state = std::make_shared<BlobReadState>(std::move(blob), size, pageSize, std::move(ioWorker));
  return CreateObservable<MessageSequence>(
      [state](rxcpp::subscriber<MessageSequence> outer) {
        ProvideBlobBatch(state, outer);
      });
}

}
