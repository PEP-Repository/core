#include <pep/weblib/ObservableStream.hpp>

#include <pep/weblib/ObservableStreamHelpers.hpp>

#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Exceptions.hpp>

#include <boost/noncopyable.hpp>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <rxcpp/rx-lite.hpp>

using namespace emscripten;
using namespace pep;

namespace {
const std::string LOG_TAG("ObservableStream");

class StreamSource : public boost::noncopyable {
  rxcpp::observable<val> data_;
  rxcpp::composite_subscription subscription_ = rxcpp::composite_subscription::empty();
  /// <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController">ReadableStreamDefaultController</a>
  val controller_;

public:
  val self;

  explicit StreamSource(rxcpp::observable<val> data) : data_(std::move(data)) {}

  void deleteSelf() {
    self.set("cancel", val::undefined()); // Prevent calling into freed object
    self.call<void>("delete"); // Deletes `this`, we must not access members afterwards
    LOG(LOG_TAG, verbose) << this << " deleted self";
  }

  /// \param controller <a href="https://developer.mozilla.org/en-US/docs/Web/API/ReadableStreamDefaultController">ReadableStreamDefaultController</a>
  void start(val controller) {
    LOG(LOG_TAG, verbose) << this << " start() called";
    assert(!subscription_.is_subscribed() && "Do not call start twice");
    controller_ = std::move(controller);
    auto deleted = std::make_shared<bool>(false);
    auto subscription = data_
      .subscribe(
        // on next
        [this](val chunk) noexcept {
          controller_.call<void>("enqueue", std::move(chunk));
        },
        // on error
        [this, deleted](std::exception_ptr ex) noexcept {
          LOG(LOG_TAG, debug) << this << " on_error: " << GetExceptionMessage(ex);
          try {
            std::rethrow_exception(std::move(ex));
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
          LOG(LOG_TAG, verbose) << this << " on_completed";
          controller_.call<void>("close");
          *deleted = true;
          deleteSelf();
        });
    // If the observable completes immediately, we have already been deleted here
    if (!*deleted) {
      subscription_ = std::move(subscription);
    } else {
      LOG(LOG_TAG, verbose) << this << " completed immediately in start()";
    }
  }

  void cancel(val reason) {
    LOG(LOG_TAG, debug) << this << " cancel() called, reason: " << val::global("String")(std::move(reason)).as<std::string>();
    // This is not ideal, as it may leak ClassHandles that haven't been enqueud yet.
    // However, even if we'd re-subscribe, already-enqueued objects would be lost.
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

val pep::weblib::CreateReadableStream(rxcpp::observable<val> data) {
  // See https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/ReadableStream
  //XXX `new` is workaround to not copy, see https://github.com/emscripten-core/emscripten/issues/25412
  val underlyingSource(new StreamSource(std::move(data)), allow_raw_pointers{});
  underlyingSource.as<StreamSource*>(allow_raw_pointers{})->self = underlyingSource;
  // We need this, because the ReadableStream will call the functions originally present on the object
  StreamSourceWithIndirectCancel(underlyingSource);
  return val::global("ReadableStream").new_(std::move(underlyingSource));
}
