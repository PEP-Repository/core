#pragma once

#include <pep/async/RxToSet.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

/// \brief Removes duplicate items from an observable.
/// \code
///   myObs.op(RxDistinct())
/// \endcode
/// \remark Replacement for rxcpp's own \c .distinct() method, which supports disappointingly few item types.
struct RxDistinct {
  /// \param items The observable emitting the items.
  /// \return An observable emitting only unique items, as determined by std::less<TItem>.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items
      .op(RxToSet(false))
      .flat_map([](std::shared_ptr<std::set<TItem>> set) {return rxcpp::observable<>::iterate(*set); });
  }
};

}
