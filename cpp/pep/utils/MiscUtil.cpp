#include <pep/utils/MiscUtil.hpp>

#include <cassert>
#include <cstring>

namespace pep {

std::string BoolToString(bool value) {
  return value ? "true" : "false";
}

bool StringToBool(const std::string& value) {
  return value == "true";
}

size_t FindLongestPrefixAtEnd(const std::string_view haystack, std::string_view needle) {
  for (needle = needle.substr(0, haystack.size());;
       needle = needle.substr(0, needle.size() - 1)) {
    if (haystack.ends_with(needle)) {
      return needle.size();
    }
  }
}

std::vector<char> ToCharVector(std::string_view in) {
  return std::vector<char>(in.begin(), in.end());
}

size_t FillVectorToCapacity(std::vector<std::string>& dest, const std::vector<std::string>& source, const size_t& cap, const size_t& offset, const size_t& padding) {
  assert(offset == 0 || offset < source.size());
  size_t destLength{ 0 };
  auto sourceIterator = source.cbegin() + static_cast<std::ptrdiff_t>(offset);
  while (sourceIterator != source.cend() && destLength + sourceIterator->length() + padding <= cap) {
    dest.push_back((*sourceIterator));
    destLength += sourceIterator->length() + padding;
    sourceIterator++;
  }
  return destLength;
}

} // namespace pep
