#pragma once

#include <memory>
#include <rxcpp/rx-lite.hpp>

namespace pep {

/// Converts shared_ptr instances emitted by an observable to shared_ptr instances of another type: \c myObs.Op(RxSharedPtrCast<RequiredType>()).
/// The source item type must be reference convertible to the destination item type.
/// \tparam TDestination The desired (destination) item type.
template <typename TDestination>
struct RxSharedPtrCast {
  /// \param items The observable emitting the item.
  /// \return An observable emitting shared_ptr instances to the destination type.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<TDestination>> operator()(rxcpp::observable<std::shared_ptr<TItem>, SourceOperator> items) const {
    static_assert(std::is_convertible<TItem&, TDestination&>::value, "The item type of the source observable must be reference convertible to the destination item type");
    return items.map([](std::shared_ptr<TItem> item) -> std::shared_ptr<TDestination> {return item; });
  }
};

}
