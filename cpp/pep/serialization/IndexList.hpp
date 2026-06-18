#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <utility>
#include <compare>
#include <ranges>

namespace pep {
class IndexList {
public:
  IndexList() = default;

  explicit IndexList(std::vector<uint32_t> indices) : indices_(std::move(indices)) {}

  std::strong_ordering operator<=>(const IndexList& other) const = default;

  std::vector<uint32_t> indices_;
};

// This is a free function such that we can enforce that we don't keep a reference to an rvalue \c IndexList
/// \note It is enforced that \p range is an lvalue
/// \throw std::out_of_range if an index is out of range
auto SafeIndexInto(std::ranges::input_range auto&& indices, std::ranges::random_access_range auto&& range)
  requires (std::ranges::sized_range<decltype(range)> && std::ranges::borrowed_range<decltype(range)>) {
  using namespace std::ranges;
  auto len = size(range);
  return std::forward<decltype(indices)>(indices)
      | views::transform([&range, len](uint32_t index) -> decltype(auto) {
        if (index >= len) {
          throw std::out_of_range("SafeIndexInto");
        }
        return range[index];
      });
}
}
