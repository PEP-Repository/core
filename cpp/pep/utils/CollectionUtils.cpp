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
  for (auto i = source.cbegin() + offset; i != source.cend(); ++i) {
    auto add = i->length() + padding;
    if (destLength + add > cap) {
      break;
    }
    dest.push_back(*i);
    destLength += add;
  }
  return destLength;
}
