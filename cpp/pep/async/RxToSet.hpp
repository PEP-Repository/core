#pragma once

#include <memory>
#include <set>
#include <unordered_set>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {
namespace detail {
template <template <typename...> class SetType>
class RxToSetImpl {
private:
  bool throwOnDuplicate_;

public:
  explicit inline RxToSetImpl(bool throwOnDuplicate = true)
    : throwOnDuplicate_(throwOnDuplicate) {
  }

  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<set<TItem>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<SetType<TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<SetType<TItem>>(),
      [throwOnDuplicate = throwOnDuplicate_](std::shared_ptr<SetType<TItem>> set, auto&& item) {
        auto added = set->emplace(std::forward<decltype(item)>(item)).second;
        if (throwOnDuplicate && !added) {
          throw std::runtime_error("Could not insert duplicate item into set");
        }
        return set;
      });
  }
};
}

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single \c std::set<>.
/// \code
///   myObservable.op(RxToSet()).
/// \endcode
class RxToSet : public detail::RxToSetImpl<std::set> {
public:
  using RxToSetImpl::RxToSetImpl;
};

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single \c std::unordered_set<>.
/// \code
///   myObservable.op(RxToSet()).
/// \endcode
class RxToUnorderedSet : public detail::RxToSetImpl<std::unordered_set> {
public:
  using RxToSetImpl::RxToSetImpl;
};

}
