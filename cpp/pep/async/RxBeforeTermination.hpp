#pragma once

#include <functional>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace pep {

namespace detail {

/// Implementor class for RxBeforeTermination<> function (below).
class RxBeforeTerminationOperator {
public:
  using Handler = std::function<void(std::optional<std::exception_ptr>)>;
private:
  Handler mHandle;

public:
  explicit RxBeforeTerminationOperator(const Handler& handle)
    : mHandle(handle) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.tap(
      [](const TItem&) {/*ignore*/},
      [handle = mHandle](std::exception_ptr ep) {handle(ep); },
      [handle = mHandle]() {handle(std::nullopt); }
    );
  }
};

}


/*! \brief Invokes a callback when an observable has finished emitting items: either because it's done, or because an error occurred.
 * \tparam THandle The callback type, which must be convertible to a function<> accepting an std::optional<std::exception_ptr> parameter and returning void.
 */
template <typename THandle>
detail::RxBeforeTerminationOperator RxBeforeTermination(const THandle& handle) {
  return detail::RxBeforeTerminationOperator(detail::RxBeforeTerminationOperator::Handler(handle)); // Conversion to Handler ensures our handler has the correct signature
}

}
