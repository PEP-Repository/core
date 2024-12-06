#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace pep {

uint32_t readBinary(std::istream& in, uint32_t defaultValue);
uint64_t readBinary(std::istream& in, uint64_t defaultValue);
std::string readBinary(std::istream& in, const std::string& defaultValue);

template <typename T>
std::vector<T> readBinary(std::istream& in, const std::vector<T>& defaultValue) {
  uint32_t size = readBinary(in, uint32_t{0});
  std::vector<T> retval;
  retval.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    retval.push_back(readBinary(in, T()));
  }
  return in.good() ? retval : defaultValue;
}

template <typename K, typename V>
std::map<K,V> readBinary(std::istream& in, const std::map<K, V>& defaultValue) {
  uint32_t size = readBinary(in, uint32_t{0});
  std::map<K,V> retval;
  for (size_t i = 0; i < size; ++i) {
    auto k = readBinary(in, K());
    auto v = readBinary(in, V());
    retval[std::move(k)] = std::move(v);
  }
  return in.good() ? retval : defaultValue;
}

void writeBinary(std::ostream& out, uint32_t number);
void writeBinary(std::ostream& out, uint64_t number);
void writeBinary(std::ostream& out, const std::string& str);

template <typename T>
void writeBinary(std::ostream& out, const std::vector<T>& vector) {
  writeBinary(out, uint32_t(vector.size()));
  for (const auto& e : vector) {
    writeBinary(out, e);
  }
}

template <typename K, typename V>
void writeBinary(std::ostream& out, const std::map<K, V>& map) {
  writeBinary(out, uint32_t(map.size()));
  for (const auto& e : map) {
    writeBinary(out, e.first);
    writeBinary(out, e.second);
  }
}

} // namespace pep
