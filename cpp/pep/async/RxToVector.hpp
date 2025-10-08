#pragma once

#include <memory>
#include <vector>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single vector.
/// \code
///   myObservable.op(RxToVector())
/// \endcode
struct RxToVector {
  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<vector<TItem>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::vector<TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::vector<TItem>>(),
      [](std::shared_ptr<std::vector<TItem>> result, const TItem& item) {
      result->push_back(item);
      return result;
    });
  }
};

}
