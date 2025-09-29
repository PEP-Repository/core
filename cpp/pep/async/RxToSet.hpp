#pragma once

#include <memory>
#include <set>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single \c std::set<>.
/// \code
///   myObservable.op(RxToSet()).
/// \endcode
struct RxToSet {
private:
  bool mThrowOnDuplicate;

public:
  explicit inline RxToSet(bool throwOnDuplicate = true)
    : mThrowOnDuplicate(throwOnDuplicate) {
  }

  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<set<TItem>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::set<TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::set<TItem>>(),
      [throwOnDuplicate = mThrowOnDuplicate](std::shared_ptr<std::set<TItem>> set, auto&& item) {
        auto added = set->emplace(std::forward<decltype(item)>(item)).second;
        if (throwOnDuplicate && !added) {
          throw std::runtime_error("Could not insert duplicate item into set");
        }
        return set;
      });
  }
};

}
