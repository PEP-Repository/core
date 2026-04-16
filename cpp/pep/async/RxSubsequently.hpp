#pragma once

#include <pep/async/CreateObservable.hpp>
#include <rxcpp/operators/rx-concat.hpp>

namespace pep {

/// @brief Invokes a callback after a primary observable has been exhausted
/// @remark As opposed to RxBeforeTermination and RxBeforeCompletion, this operator invokes its callback _after_ the
///         primary observable's resources have been released.
class RxSubsequently {
private:
  std::function<void()> callback_;

  /// @brief Produces an empty observable that invokes our callback when subscribed.
  /// @tparam TItem The observable's formal item type.
  /// @return An empty observable that invokes our callback upon subscription.
  template <typename TItem>
  rxcpp::observable<TItem> makeTail() const {
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    return CreateObservable<TItem>([callback = callback_](rxcpp::subscriber<TItem> subscriber) {
      callback();
      subscriber.on_completed();
      });
  }

public:
  /// @brief Invoke
  /// @tparam TInvoke A parameterless function-like type
  /// @param callback A callback to invoke after a primary observable has been exhausted
  template <typename TInvoke>
  explicit RxSubsequently(TInvoke callback)
    : callback_(std::move(callback)) {
  }

  /// @brief Invokes the callback after the specified (primary) observable has been exhausted
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  /// @param items The observable to exhaust before the callback is invoked
  /// @return An observable emitting the original (primary) observable's items
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items
      .concat(this->makeTail<TItem>()); // (Ab)use the .concat operator to ensure that the callback is invoked _after_ the primary observable has been exhausted
  }
};

}
