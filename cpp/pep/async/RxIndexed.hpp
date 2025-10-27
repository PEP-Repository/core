#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace pep {

/// Add indices to each item using std::pair, starting at 0
template <typename TIndex = size_t>
struct RxIndexed {
  template <typename TItem, typename SourceOperator>
  auto operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    // Not thread-safe
    auto index = std::make_shared<TIndex>();
    return items.map([index](TItem item) { return std::pair<TIndex, TItem>{(*index)++, std::move(item)}; });
  }
};

}
