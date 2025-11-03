#pragma once

#include <pep/async/CreateObservable.hpp>

#include <ranges>

namespace pep {

/// Like \c rxcpp::observable<>::iterate, but moves elements out of the container, as it's single-use.
/// Note that you still have to move the container yourself,
/// which means you can always safely use this instead of \c rxcpp::observable<>::iterate.
/// Note that \c rxcpp::observable<>::iterate with move_iterators does not work, as it makes the collection const and
/// retrieves the iterator type using \c decltype(std::begin(declval<C>())), which makes move iterator values const.
auto RxMoveIterate(std::ranges::input_range auto container) {
  using namespace std::ranges;
  using TValue = range_value_t<decltype(container)>;
  return CreateObservable<TValue>([container = std::make_shared<decltype(container)>(std::move(container))]
      (const rxcpp::subscriber<TValue>& subscriber) {
    for (auto& elem : *container) {
      if (!subscriber.is_subscribed()) { break; }
      subscriber.on_next(std::move(elem));
    }
    if (subscriber.is_subscribed()) {
      subscriber.on_completed();
    }
  });
}

}
