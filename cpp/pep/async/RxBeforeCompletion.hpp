#pragma once

#include <pep/async/RxBeforeTermination.hpp>

namespace pep {

/// \brief Invokes a callback when an observable has successfully finished emitting items.
struct RxBeforeCompletion {
public:
  using Handler = std::function<void()>;

private:
  Handler mHandler;

public:
  explicit RxBeforeCompletion(Handler handler)
    : mHandler(std::move(handler)) {
  }

  /// \param items The source observable.
  /// \return An observable emitting the same items as the source observable.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    return items.op(RxBeforeTermination([handler = mHandler](std::optional<std::exception_ptr> error) {
      if (!error.has_value()) {
        handler();
      }
      }));
  }
};

}
