#include <pep/utils/CollectionUtils.hpp>

#include <cassert>

size_t pep::FindLongestPrefixAtEnd(const std::string_view haystack, std::string_view needle) {
  for (needle = needle.substr(0, haystack.size());;
       needle = needle.substr(0, needle.size() - 1)) {
    if (haystack.ends_with(needle)) {
      return needle.size();
    }
  }
}

size_t pep::FillVectorToCapacity(std::vector<std::string>& dest, const std::vector<std::string>& source,
    const size_t& cap, const size_t& offset, const size_t& padding) {
  assert(offset == 0 || offset < source.size());
  size_t destLength{0};
  auto sourceIterator = source.cbegin() + static_cast<std::ptrdiff_t>(offset);
  while (sourceIterator != source.cend() && destLength + sourceIterator->length() + padding <= cap) {
    dest.push_back((*sourceIterator));
    destLength += sourceIterator->length() + padding;
    sourceIterator++;
  }
  return destLength;
}
