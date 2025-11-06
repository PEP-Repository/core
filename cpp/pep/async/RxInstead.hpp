#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-ignore_elements.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <cassert>

namespace pep {

namespace detail {

/// Implementor class for RxInstead<> function (below).
template <typename T>
struct RxInsteadOperator {
private:
  T mReplacement;

public:
  explicit RxInsteadOperator(const T& replacement)
    : mReplacement(replacement) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<T> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    static_assert(!rxcpp::is_observable<TItem>::value,
                  "RxInstead used on observable<observable<T>>, which would not wait for the inner observables; "
                  "you probably forgot a flat_map");
    return items
      .ignore_elements()
      .reduce(
        mReplacement,
        [](const T& replacement, const TItem&) {assert(false); return  replacement; } // Should never be called due to .ignore_elements() above
      );
  }
};

}


/*! \brief Exhausts a source observable, then emits a single (specified) item.
/// \code
///   myObs.op(RxInstead(justThisItem))
/// \endcode
 * \param item The item to emit instead of the source observable's items.
 * \remark Mainly intended to help with collections that cannot (easily) be constructed by means of .reduce.
 */
template <typename T>
detail::RxInsteadOperator<T> RxInstead(const T& item) {
  return detail::RxInsteadOperator<T>(item);
}

}
