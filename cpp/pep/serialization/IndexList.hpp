#pragma once

#include <cstdint>
#include <vector>

namespace pep {

class IndexList {
public:
  inline IndexList() {}
  explicit inline IndexList(const std::vector<uint32_t>& indices) :
    mIndices(indices) { }

  std::strong_ordering operator<=>(const IndexList& other) const = default;

  std::vector<uint32_t> mIndices;
};

}
