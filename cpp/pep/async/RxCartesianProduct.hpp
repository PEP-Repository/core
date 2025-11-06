#pragma once

#include <pep/async/RxIterate.hpp>
#include <pep/async/RxToVector.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

namespace detail {

// Implementor class for RxCartesianProduct<> function (below)
template <typename TItem2, typename SourceOperator2>
class RxCartesianProductOperator {
private:
  rxcpp::observable<TItem2, SourceOperator2> mObservable2;

public:
  explicit RxCartesianProductOperator(rxcpp::observable<TItem2, SourceOperator2> observable2)
    : mObservable2(observable2) {
  }

  /// \param o1 The first observable
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::pair<TItem, TItem2>> operator()(rxcpp::observable<TItem, SourceOperator> o1) const {
    return mObservable2
      .op(RxToVector()) // Collect o2's items into a (single) vector to prevent o2 from being subscribed multiple times: see #1070
      .flat_map([o1](std::shared_ptr<std::vector<TItem2>> v2) {
      // Adapted from https://stackoverflow.com/a/26588822
      return o1
        .flat_map([v2 = std::move(v2)](TItem i1) {
        return RxIterate(std::move(*v2))
          .map([i1](TItem2 i2) {return std::make_pair(std::move(i1), std::move(i2)); });
      });
    });
  }
};

}


/*! \brief Combines each emission of one observable with each emission of another observable.
 *
 * \param o2 The second observable
 * \return An observable<> of std::pair<> instances for each combination of items emitted by the source observable and and o2
 */
template <typename T2, typename SourceOperator2>
detail::RxCartesianProductOperator<T2, SourceOperator2> RxCartesianProduct(rxcpp::observable<T2, SourceOperator2> o2) {
  return detail::RxCartesianProductOperator<T2, SourceOperator2>(o2);
}

}
