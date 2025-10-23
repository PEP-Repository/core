#pragma once

#include <pep/utils/Shared.hpp>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace pep {

/// Makes sure you get one and only one item back from an RX call.
struct RxGetOne {
  std::string errorText;

  /// \param items The observable emitting the item.
  /// \return An observable emitting a single TItem.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    auto emitted = MakeSharedCopy(false);
    return items.tap(
      [emitted, errorText = this->errorText](const TItem&) {
        if (*emitted) {
          throw std::runtime_error("Encountered multiple " + errorText);
        }
        *emitted = true;
      },
      [](std::exception_ptr) { /* let it escape */},
      [emitted, errorText = this->errorText]() {
        if (!*emitted) {
          throw std::runtime_error("Encountered no " + errorText);
        }
      }
    );
  }

  /// \param errorText Custom text to be displayed in the errors when no of multiple items are found.
  RxGetOne(std::string errorText) : errorText(std::move(errorText)) {}
};

}
