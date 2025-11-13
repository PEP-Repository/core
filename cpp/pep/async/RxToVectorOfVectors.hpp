#pragma once

#include <pep/utils/VectorOfVectors.hpp>
#include <memory>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

/// \brief Aggregates the vector<TItem> emissions of an observable into (an observable emitting) (a shared pointer to) a single VectorOfVectors<TItem>.
/// \code
///   myObservable.op(RxToVectorOfVectors())
/// \endcode
struct RxToVectorOfVectors {
  /// \param items The observable emitting individual vector<TItem> instances.
  /// \return An observable emitting a single shared_ptr<VectorOfVectors<TItem>>.
  /// \tparam TItem The type of item stored in the observable's vector instances.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<VectorOfVectors<TItem>>> operator()(rxcpp::observable<std::vector<TItem>, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<VectorOfVectors<TItem>>(),
      [](std::shared_ptr<VectorOfVectors<TItem>> result, std::vector<TItem> single) {
        *result += std::move(single);
        return result;
      });
  }
};

}
