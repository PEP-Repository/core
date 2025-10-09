#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/async/RxToEmpty.hpp>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace pep {

/// \brief Verifies that the an observable emits a specified minimum and/or maximum number of items.
/// \code
///   myObs.op(RxRequireCount(2U, 46U))
/// \endcode
struct RxRequireCount {
private:
  size_t mMin;
  size_t mMax;

  static void ValidateMax(size_t count, size_t max);
  static rxcpp::observable<FakeVoid> ValidateMin(std::shared_ptr<size_t> count, size_t min);

public:
  RxRequireCount(size_t min, size_t max) noexcept : mMin(min), mMax(max) {}
  explicit RxRequireCount(size_t exact) noexcept : RxRequireCount(exact, exact) {}

  /// \param items The observable emitting the items.
  /// \return An observable emitting the source observable's items.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    auto count = std::make_shared<size_t>(0U);
    return items
      .tap(
        [count, max = mMax](const TItem&) { ValidateMax(++*count, max); } // We can just throw an exception from on_next...
        // ... but throwing from on_complete produces weird behavior, so we...
      )
      .concat(ValidateMin(count, mMin) // ... append an empty observable<FakeVoid> that performs the validation instead...
        .op(RxToEmpty<TItem>())); // ... and convert that observable<FakeVoid> to an observable<TItem> that we can append
  }
};

/// \brief Verifies that an observable emits at least one item.
/// \code
///   myObs.op(RxRequireNonEmpty())
/// \endcode
struct RxRequireNonEmpty : public RxRequireCount {
  RxRequireNonEmpty() noexcept : RxRequireCount(1U, std::numeric_limits<size_t>::max()) {}
};

}
