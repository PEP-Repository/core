#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-ignore_elements.hpp>
#include <cassert>

namespace pep {

/// \brief Exhausts an observable of one item type and switches processing to (an empty observable of) another item type
/// \tparam TDestination The item type to switch to
/// \code
///   myObs.op(RxToEmpty<SomeDifferentType>()).concat(myDifferentValues)
/// \endcode
template <typename TDestination>
struct RxToEmpty {
  /// \param items The observable emitting the items.
  /// \return An empty observable<TDestination>.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TDestination> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items
      .ignore_elements()
      .concat_map([](const TItem&) -> rxcpp::observable<TDestination> {
      assert(false); // Should never be called due to .ignore_elements() above
      return rxcpp::observable<>::empty<TDestination>();
        });
  }
};

}
