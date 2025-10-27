#include <pep/weblib/ObservableStream.hpp>

#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Exceptions.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>

using namespace emscripten;
using namespace pep;

namespace {
const std::string LOG_TAG("ObservableStream");

class StreamSource {
  rxcpp::observable<val> data_;
  rxcpp::composite_subscription subscription_ = rxcpp::composite_subscription::empty();
  val controller_;

public:
  val self;

  explicit StreamSource(rxcpp::observable<val> data) : data_(std::move(data)) {}

  void deleteSelf() {
    self.set("cancel", val::undefined()); // Prevent calling into freed object
    self.call<void>("delete"); // Deletes `this`, we must not access members afterwards
    LOG(LOG_TAG, debug) << this << " deleted self";
  }

  /// \param controller https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController
  void start(const val& controller) {
    LOG(LOG_TAG, debug) << this << " start() called";
    assert(!subscription_.is_subscribed() && "Do not call start twice");
    controller_ = controller;
    subscription_ = data_
      .subscribe(
        // on next
        [this](val chunk) noexcept {
          controller_.call<void>("enqueue", std::move(chunk));
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
};
}

EMSCRIPTEN_BINDINGS(ObservableStream) {
  class_<StreamSource>("StreamSource")
    .function("start", &StreamSource::start)
    .function("cancel", &StreamSource::cancel)
    ;
}

val pep::CreateReadableStream(rxcpp::observable<val> data) {
  // https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/ReadableStream
  val underlyingSource(StreamSource(std::move(data)));
  underlyingSource.as<StreamSource*>(allow_raw_pointers{})->self = underlyingSource;
  // We need withIndirectCancel, because the ReadableStream will call the functions originally present on the object
  return val::global("ReadableStream").new_(val::module_property("withIndirectCancel")(std::move(underlyingSource)));
}
