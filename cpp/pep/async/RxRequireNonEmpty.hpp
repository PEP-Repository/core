#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace pep {

namespace detail {

/// \brief Provides an observable's number of items to a callback function.
/// \code
///   myObs.op(RxProvideCount([](size_t size) { std::cout << size << " items"; }))
/// \endcode
/// If the source observable emits an error, the callback is not invoked.
struct RxProvideCount {
  using Handler = std::function<void(size_t)>;

private:
  Handler mHandler;

public:
  template <typename THandler>
  explicit inline RxProvideCount(const THandler& handler)
    : mHandler(handler) {}

  /// \param items The observable emitting the items.
  /// \return An observable emitting the source observable's items.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    auto count = std::make_shared<size_t>(0U);
    return items
      .tap(
        [count](const TItem&) {++*count; },
        [](std::exception_ptr) { /* Don't invoke handler, e.g. making it think that the source (successfully) emitted no items */ },
        [count, handler = mHandler] {handler(*count); }
      );
  }
};

} // End namespace detail

/// \brief Verifies that an observable emits at least one item.
/// \code
///   myObs.op(RxRequireNonEmpty())
/// \endcode
struct RxRequireNonEmpty : public detail::RxProvideCount {
public:
  /// \param assertOnly Whether to validate the source observable using an assertion (true) or a run time check (false).
  explicit RxRequireNonEmpty(bool assertOnly = false);
};

}
