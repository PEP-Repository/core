#include <pep/weblib/ObservableByteStream.hpp>

#include <pep/weblib/ObservableStreamHelpers.hpp>

#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Exceptions.hpp>

#include <boost/noncopyable.hpp>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>

#include <atomic>

using namespace emscripten;
using namespace pep;

namespace {
const std::string LogTag("ObservableByteStream");

class ByteStreamSource : public boost::noncopyable {
  static std::atomic_flag warnedNoByobSupport;

  rxcpp::observable<std::string> data_;
  std::size_t chunkSize_;
  rxcpp::composite_subscription subscription_ = rxcpp::composite_subscription::empty();
  /// <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableByteStreamController">ReadableByteStreamController</a>
  /// | <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController">ReadableStreamDefaultController</a>
  val controller_;

public:
  val self;

  explicit ByteStreamSource(rxcpp::observable<std::string> data, std::size_t chunkSize)
      : data_(std::move(data)), chunkSize_{chunkSize} {}

  void deleteSelf() {
    self.set("cancel", val::undefined()); // Prevent calling into freed object
    self.call<void>("delete"); // This deletes this, we must not access members after this
    PEP_LOG(LogTag, Severity::Verbose) << "deleted self";
  }

  /// \param controller
  ///   <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableByteStreamController">ReadableByteStreamController</a>
  ///   | <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController">ReadableStreamDefaultController</a>
  void start(val controller) {
    PEP_LOG(LogTag, Severity::Verbose) << this << " start() called";
    assert(!subscription_.is_subscribed() && "Do not call start twice");
    controller_ = std::move(controller);
    val byteStreamControllerClass = val::global("ReadableByteStreamController");
    if ((!byteStreamControllerClass || !controller_.instanceof(byteStreamControllerClass))
        && !warnedNoByobSupport.test_and_set()) {
      PEP_LOG(LogTag, Severity::Debug) << "ReadableStream BYOB mode not supported by browser, using less-efficient buffer-copying method";
    }
    auto deleted = std::make_shared<bool>(false);
    auto subscription = data_
      .subscribe(
        // on next
        [this](std::string_view chunk) noexcept {
          std::span chunkSpan = pep::ConvertBytes<std::uint8_t>(chunk);

          // ReadableStreamBYOBRequest | null | undefined (if unsupported).
          // https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamBYOBRequest
          val byobRequest = controller_["byobRequest"];

          // Uint8Array<SharedArrayBuffer>
          val chunkView(typed_memory_view(chunkSpan.size(), chunkSpan.data()));
          if (!!byobRequest) {
            // Uint8Array<ArrayBuffer>
            val byobBuf = byobRequest["view"];
            auto byobSize = byobBuf["length"].as<std::size_t>();
            if (byobSize >= chunkSpan.size()) {
              // Copy to provided buffer
              byobBuf.call<void>("set", std::move(chunkView));
              byobRequest.call<void>("respond", chunkSpan.size());
            } else {
              PEP_LOG(LogTag, Severity::Debug) << this << " Provided buffer too small for chunk: "
                  << byobSize << "<" << chunkSpan.size() << ", allocating extra buffer";

              std::span
                  chunkSpan1 = chunkSpan.subspan(0, byobSize),
                  chunkSpan2 = chunkSpan.subspan(byobSize);

              // Uint8Array<SharedArrayBuffer>
              val chunkView1(typed_memory_view(chunkSpan1.size(), chunkSpan1.data())),
                  chunkView2(typed_memory_view(chunkSpan2.size(), chunkSpan2.data()));

              // Copy what fits into provided buffer
              byobBuf.call<void>("set", std::move(chunkView1));
              byobRequest.call<void>("respond", chunkSpan1.size());

              // Copy rest into new buffer
              controller_.call<void>("enqueue",
                  val::global("Uint8Array").new_(std::move(chunkView2)));
            }
          } else {
            // Only log if the browser supports BYOB mode
            if (byobRequest.isNull()) {
              PEP_LOG(LogTag, Severity::Debug) << this << " No buffer provided, allocating new buffer";
            }

            // Copy into new buffer
            controller_.call<void>("enqueue",
                val::global("Uint8Array").new_(std::move(chunkView)));
          }
        },
        // on error
        [this, deleted](std::exception_ptr ex) noexcept {
          PEP_LOG(LogTag, Severity::Debug) << this << " on_error: " << GetExceptionMessage(ex);
          try {
            std::rethrow_exception(std::move(ex)); //NOLINT(performance-move-const-arg) libc++ doesn't support moving exception_ptr
          } catch (...) {
            // Listener should decrement exception reference count when handled
            //XXX Uses internal API, see https://github.com/emscripten-core/emscripten/issues/25963
            controller_.call<void>("error", val::take_ownership(emscripten::internal::_emval_from_current_cxa_exception()));
          }
          *deleted = true;
          deleteSelf();
        },
        // on completed
        [this, deleted]() noexcept {
           PEP_LOG(LogTag, Severity::Verbose) << this << " on_completed";
           controller_.call<void>("close");
           *deleted = true;
           deleteSelf();
        });
    // If the observable completes immediately, we have already been deleted here
    if (!*deleted) {
      subscription_ = std::move(subscription);
    } else {
      PEP_LOG(LogTag, Severity::Verbose) << this << " completed immediately in start()";
    }
  }

  void cancel(const val& reason) {
    PEP_LOG(LogTag, Severity::Debug) << this << " cancel() called, reason: " << val::global("String")(reason).as<std::string>();
    subscription_.unsubscribe();
    deleteSelf();
  }

  // Enable BYOB mode
  std::string type() const { return "bytes"; }
  std::size_t autoAllocateChunkSize() const { return chunkSize_; }
};

std::atomic_flag ByteStreamSource::warnedNoByobSupport;
}

EMSCRIPTEN_BINDINGS(ObservableByteStream) {
  class_<ByteStreamSource>("ByteStreamSource")
    .function("start", &ByteStreamSource::start)
    .function("cancel", &ByteStreamSource::cancel)
    .property("type", &ByteStreamSource::type)
    .property("autoAllocateChunkSize", &ByteStreamSource::autoAllocateChunkSize)
    ;
}

val pep::weblib::CreateReadableByteStream(rxcpp::observable<std::string> data, std::size_t chunkSize) {
  // See https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/ReadableStream
  //XXX `new` is workaround to not copy, see https://github.com/emscripten-core/emscripten/issues/25412
  val underlyingSource(new ByteStreamSource(std::move(data), chunkSize), allow_raw_pointers{});
  underlyingSource.as<ByteStreamSource*>(allow_raw_pointers{})->self = underlyingSource;
  // We need this, because the ReadableStream will call the functions originally present on the object
  StreamSourceWithIndirectCancel(underlyingSource);
  return val::global("ReadableStream").new_(std::move(underlyingSource));
}
