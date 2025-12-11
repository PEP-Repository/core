#pragma once

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-map.hpp>

#include <optional>

namespace pep {

struct RxFilterNullopt {
  template <typename TItem, typename SourceOperator>
  auto operator()(rxcpp::observable<std::optional<TItem>, SourceOperator> items) const {
    return items
      .filter([](const std::optional<TItem>& opt) { return opt.has_value(); })
      .map([](std::optional<TItem> opt) { return std::move(*opt); });
  }
};

}
