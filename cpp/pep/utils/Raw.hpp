#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace pep {

uint32_t ReadBinary(std::istream& in, uint32_t defaultValue);
uint64_t ReadBinary(std::istream& in, uint64_t defaultValue);
std::string ReadBinary(std::istream& in, const std::string& defaultValue);

template <typename T>
std::vector<T> ReadBinary(std::istream& in, const std::vector<T>& defaultValue) {
  uint32_t size = ReadBinary(in, uint32_t{0});
  std::vector<T> retval;
  retval.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    retval.push_back(ReadBinary(in, T()));
  }
  return in.good() ? retval : defaultValue;
}

template <typename K, typename V>
std::map<K,V> ReadBinary(std::istream& in, const std::map<K, V>& defaultValue) {
  uint32_t size = ReadBinary(in, uint32_t{0});
  std::map<K,V> retval;
  for (size_t i = 0; i < size; ++i) {
    auto k = ReadBinary(in, K());
    auto v = ReadBinary(in, V());
    retval[std::move(k)] = std::move(v);
  }
  return in.good() ? retval : defaultValue;
}

void WriteBinary(std::ostream& out, uint32_t number);
void WriteBinary(std::ostream& out, uint64_t number);
void WriteBinary(std::ostream& out, const std::string& str);

template <typename T>
void WriteBinary(std::ostream& out, const std::vector<T>& vector) {
  WriteBinary(out, uint32_t(vector.size()));
  for (const auto& e : vector) {
    WriteBinary(out, e);
  }
}

template <typename K, typename V>
void WriteBinary(std::ostream& out, const std::map<K, V>& map) {
  WriteBinary(out, uint32_t(map.size()));
  for (const auto& e : map) {
    WriteBinary(out, e.first);
    WriteBinary(out, e.second);
  }
}

} // namespace pep
