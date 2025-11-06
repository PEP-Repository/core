
#include <pep/utils/Bitpacking.hpp>

namespace pep {

std::string PackUint8(uint8_t x) {
  return std::string{static_cast<char>(x)};
}

std::string PackUint32BE(uint32_t x) {
  return std::string{
    static_cast<char>(x >> 24), static_cast<char>(x >> 16), static_cast<char>(x >> 8),  static_cast<char>(x >> 0)};
}

// Converts uint64 to std::string big endian.
std::string PackUint64BE(uint64_t x) {
  return std::string{
    static_cast<char>(x >> 56), static_cast<char>(x >> 48), static_cast<char>(x >> 40), static_cast<char>(x >> 32),
    static_cast<char>(x >> 24), static_cast<char>(x >> 16), static_cast<char>(x >> 8),  static_cast<char>(x >> 0)};
}

// Unpacks an uint32 from (the first 4 bytes of) a big-endian string.
uint32_t UnpackUint32BE(std::string_view str) {
  // copy first 4 characters of string (does not allocate because of Short-String-Optimization)
  std::string x(str, 0, 4);

  // ensure string has at least 4 characters, fill with 0 (null) otherwise
  if (x.size() < 4) {
    x.resize(4, 0);
  }

  return (
          ((uint32_t{static_cast<uint8_t>(x[3])}) << 0)
        | ((uint32_t{static_cast<uint8_t>(x[2])}) << 8)
        | ((uint32_t{static_cast<uint8_t>(x[1])}) << 16)
        | ((uint32_t{static_cast<uint8_t>(x[0])}) << 24)
      );
}

// Unpacks an uint64 from (the first 8 bytes of) a big-endian string.
uint64_t UnpackUint64BE(std::string_view str) {
  // copy first 8 characters of string (does not allocate because of Short-String-Optimization)
  std::string x(str, 0, 8);

  // ensure string has at least 8 characters, fill with 0 (null) otherwise
  if (x.size() < 8) {
    x.resize(8, 0);
  }

  return (
          ((uint64_t{static_cast<uint8_t>(x[7])}) << 0)
        | ((uint64_t{static_cast<uint8_t>(x[6])}) << 8)
        | ((uint64_t{static_cast<uint8_t>(x[5])}) << 16)
        | ((uint64_t{static_cast<uint8_t>(x[4])}) << 24)
        | ((uint64_t{static_cast<uint8_t>(x[3])}) << 32)
        | ((uint64_t{static_cast<uint8_t>(x[2])}) << 40)
        | ((uint64_t{static_cast<uint8_t>(x[1])}) << 48)
        | ((uint64_t{static_cast<uint8_t>(x[0])}) << 56)
      );
}

} // namespace pep
