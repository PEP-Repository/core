#pragma once

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <functional>
#include <optional>
#include <utility>

#include <rxcpp/rx-lite.hpp>

namespace pep {

namespace detail {

/// The type of callback accepted by RxBeforeTermination<> (below).
using RxBeforeTerminationHandler = std::function<void(std::optional<std::exception_ptr>)>;

/// \brief Produces the subscribers that RxBeforeTerminationOperator (below) lifts into an observable.
/// \tparam TItem The type of item produced by the observable.
/// \remark This does what rxcpp's tap<> does, but tap<> lets an exception from the handler escape into
///   whatever code invoked the terminal notification, which is (at best) an unrelated Rx operator and
///   (at worst) an I/O callback that will blame the exception on its peer. Worse, an on_completed<>
///   that throws unsubscribes every subscriber in the chain as it unwinds, so a catch<> further up can
///   no longer report the exception to its (by then detached) destination and silently discards it.
///   We therefore convert the exception into an on_error<> here, while the destination can still
///   accept one.
template <typename TItem>
class RxBeforeTerminationSubscriberFactory {
private:
  RxBeforeTerminationHandler handle_;

public:
  explicit RxBeforeTerminationSubscriberFactory(RxBeforeTerminationHandler handle)
    : handle_(std::move(handle)) {}

  rxcpp::subscriber<TItem> operator()(rxcpp::subscriber<TItem> destination) const {
    return rxcpp::make_subscriber<TItem>(
      destination.get_subscription(), // Share the destination's lifetime, as rxcpp's own operators do
      [destination](TItem item) { destination.on_next(std::move(item)); },
      [destination, handle = handle_](std::exception_ptr exception) {
        try {
          handle(exception);
        }
        catch (...) {
          // The destination accepts only a single terminal notification, and the error that we were
          // already reporting is the more informative one, so keep that one and log this one.
          PEP_LOG("RxBeforeTermination", Severity::Critical) << "Error handler threw while handling "
            << GetExceptionMessage(exception) << ": " << GetExceptionMessage(std::current_exception());
        }
        destination.on_error(exception);
      },
      [destination, handle = handle_]() {
        try {
          handle(std::nullopt);
        }
        catch (...) {
          // We haven't completed the destination yet, so it can still accept the error.
          destination.on_error(std::current_exception());
          return;
        }
        destination.on_completed();
      });
  }
};

}

/// \brief Invokes a callback when an observable has finished emitting items: either because it's done, or because an error occurred.
/// \remark The callback is invoked _before_ the observable is fully exhausted and its resources released. Also see \c RxSubsequently .
/// \remark If the callback throws while the observable is completing, the exception is reported to the subscriber as an error.
///   If it throws while the observable is already terminating with an error, the original error is reported and the callback's exception is logged.
class RxBeforeTermination {
public:
  using Handler = detail::RxBeforeTerminationHandler;

private:
  Handler handle_;

public:
  explicit RxBeforeTermination(Handler handle)
    : handle_(std::move(handle)) {}

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.template lift<TItem>(detail::RxBeforeTerminationSubscriberFactory<TItem>(handle_));
  }
};

}
