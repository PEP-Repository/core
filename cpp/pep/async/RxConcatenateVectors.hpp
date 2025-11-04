#pragma once

#include <memory>
#include <vector>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

/// \brief For a source observable that emits vector<TItem> values, aggregates the items into (an observable emitting) (a shared pointer to) a single vector.
/// \remark If you just need items to be aggregated into a single container (and not into contiguous memory), consider using the more efficient RxToVectorOfVectors instead.
/// \code
///   myObservable.op(RxConcatenateVectors()).
/// \endcode
struct RxConcatenateVectors {
  /// \param chunks The observable emitting vectors containing individual items.
  /// \return An observable emitting a single shared_ptr<vector<TItem>>.
  /// \tparam TItem The type of item stored in the vector<> produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::vector<TItem>>> operator()(rxcpp::observable<std::vector<TItem>, SourceOperator> chunks) const {
    return chunks.reduce(
      std::make_shared<std::vector<TItem>>(),
      [](std::shared_ptr<std::vector<TItem>> result, std::vector<TItem> chunk) {
      result->reserve(result->size() + chunk.size());
      result->insert(result->end(), std::move_iterator(chunk.begin()), std::move_iterator(chunk.end()));
      return result;
    });
  }
};

}
