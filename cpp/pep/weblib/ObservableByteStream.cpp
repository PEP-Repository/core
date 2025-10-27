#include <pep/weblib/ObservableByteStream.hpp>

#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Exceptions.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>

using namespace emscripten;
using namespace pep;

namespace {
const std::string LOG_TAG("ObservableByteStream");

class ByteStreamSource {
  rxcpp::observable<std::string> data_;
  std::size_t chunkSize_;
  rxcpp::composite_subscription subscription_ = rxcpp::composite_subscription::empty();
  val controller_;

public:
  val self;

  explicit ByteStreamSource(rxcpp::observable<std::string> data, std::size_t chunkSize)
      : data_(std::move(data)), chunkSize_{chunkSize} {}

  void deleteSelf() {
    self.set("cancel", val::undefined()); // Prevent calling into freed object
    self.call<void>("delete"); // This deletes this, we must not access members after this
    LOG(LOG_TAG, debug) << "deleted self";
  }

  /// \param controller https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController
  void start(const val& controller) {
    LOG(LOG_TAG, debug) << this << " start() called";
    assert(!subscription_.is_subscribed() && "Do not call start twice");
    controller_ = controller;
    subscription_ = data_
      .subscribe(
        // on next
        [this](std::string_view chunk) noexcept {
          std::span chunkSpan = pep::ConvertBytes<std::uint8_t>(chunk);

          // ReadableStreamBYOBRequest | null
          val byobRequest = controller_["byobRequest"];

          val chunkView(typed_memory_view(chunkSpan.size(), chunkSpan.data()));
          if (!!byobRequest) {
            val byobBuf = byobRequest["view"];
            auto byobSize = byobBuf["length"].as<std::size_t>();
            if (byobSize >= chunkSpan.size()) {
              // Copy to provided buffer
              byobBuf.call<void>("set", chunkView);
              byobRequest.call<void>("respond", chunkSpan.size());
            } else {
              LOG(LOG_TAG, warning) << this << " Provided buffer too small for chunk: "
                  << byobSize << "<" << chunkSpan.size() << ", allocating extra buffer";

              std::span
                  chunkSpan1 = chunkSpan.subspan(0, byobSize),
                  chunkSpan2 = chunkSpan.subspan(byobSize);

              val chunkView1(typed_memory_view(chunkSpan1.size(), chunkSpan1.data())),
                  chunkView2(typed_memory_view(chunkSpan2.size(), chunkSpan2.data()));

              // Copy what fits into provided buffer
              byobBuf.call<void>("set", chunkView1);
              byobRequest.call<void>("respond", chunkSpan1.size());

              // Copy rest into new buffer
              controller_.call<void>("enqueue",
                  val::global("Uint8Array").new_(chunkView2));
            }
          } else {
            LOG(LOG_TAG, warning) << this << " No buffer provided, allocating new buffer";

            // Copy into new buffer
            controller_.call<void>("enqueue",
                val::global("Uint8Array").new_(chunkView));
          }
        },
        // on error
        [this](std::exception_ptr ex) noexcept {
          LOG(LOG_TAG, debug) << this << " on_error: " << GetExceptionMessage(ex);
          try {
            std::rethrow_exception(std::move(ex));
          } catch (...) {
            controller_.call<void>("error", val::take_ownership(emscripten::internal::_emval_from_current_cxa_exception()));
          }
          deleteSelf();
        },
        // on completed
        [this]() noexcept {
           LOG(LOG_TAG, debug) << this << " on_completed";
           controller_.call<void>("close");
           deleteSelf();
        });
  }

  void cancel(val reason) {
    LOG(LOG_TAG, debug) << this << " cancel() called, reason: " << val::global("String")(std::move(reason)).as<std::string>();
    subscription_.unsubscribe();
    deleteSelf();
  }

  std::string type() const { return "bytes"; }

  std::size_t autoAllocateChunkSize() const { return chunkSize_; }
};
}

EMSCRIPTEN_BINDINGS(ObservableByteStream) {
  class_<ByteStreamSource>("ByteStreamSource")
    .function("start", &ByteStreamSource::start)
    .function("cancel", &ByteStreamSource::cancel)
    .property("type", &ByteStreamSource::type)
    .property("autoAllocateChunkSize", &ByteStreamSource::autoAllocateChunkSize)
    ;
}

val pep::CreateReadableByteStream(rxcpp::observable<std::string> data, std::size_t chunkSize) {
  // https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/ReadableStream
  val underlyingSource(ByteStreamSource(std::move(data), chunkSize));
  underlyingSource.as<ByteStreamSource*>(allow_raw_pointers{})->self = underlyingSource;
  // We need withIndirectCancel, because the ReadableStream will call the functions originally present on the object
  return val::global("ReadableStream").new_(val::module_property("withIndirectCancel")(std::move(underlyingSource)));
}
