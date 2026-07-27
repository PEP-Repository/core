#include <pep/crypto/ConstTime.hpp>

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace pep::const_time {

namespace {
/// Get mask in constant time that returns all 1s when \p lhs > \p rhs and all 0s otherwise.
///
/// Assumes <tt>rhs - lhs</tt> fits in 8 bits.
int MaskGt(int lhs, int rhs) {
  return (rhs - lhs) >> 8;
}

/// Convert \p nibble to hex in constant time.
/// \param nibble Nibble, assumed to be 4 bits.
char NibbleToHex(const std::uint8_t nibble) {
  assert(nibble < 16);
  // Add '0'. If nibble > 9, add 'A' - '0' - 10.
  return static_cast<char>(nibble + '0' + (MaskGt(nibble, 9) & ('A' - '0' - 10)));
}

/// Convert \p hex to nibble in constant time.
/// \returns 4-bit nibble.
/// \throws std::invalid_argument for invalid hex character. For invalid characters, the execution time may depend on the value.
std::uint8_t NibbleFromHex(char hex) {
  // Make uppercase by clearing bit 5 unless hex < 'a'
  hex = static_cast<char>(hex & (~(1 << 5) | MaskGt('a', hex)));
  // Determine if character is invalid without leaking in which half (0-9 vs A-Z) it lies.
  // Leaking why it is out-of-range is fine.
  if (hex < '0' || hex > 'F' || (MaskGt(hex, '9') & MaskGt('A', hex)) != 0) {
    throw std::invalid_argument("Invalid hex character");
  }
  // Subtract '0'. If hex >= 'A', subtract 'A' - '0' - 10.
  return static_cast<std::uint8_t>(hex - '0' - (MaskGt(hex, 'A' - 1) & ('A' - '0' - 10)));
}
}

std::string ToHex(std::string_view bytes) {
  static_assert(std::same_as<unsigned char, std::uint8_t>);
  std::string hex;
  hex.reserve(bytes.size() * 2);
  for (const char byte : bytes) {
    hex.push_back(NibbleToHex(static_cast<std::uint8_t>((byte >> 4) & 0xf)));
    hex.push_back(NibbleToHex(static_cast<std::uint8_t>(byte & 0xf)));
  }
  return hex;
}

std::string FromHex(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    throw std::invalid_argument("Invalid hex length");
  }
  std::string bytes;
  bytes.reserve(hex.size() / 2);
  for (auto hexIt = hex.begin(); hexIt != hex.end(); hexIt += 2) {
    bytes.push_back(static_cast<char>((NibbleFromHex(hexIt[0]) << 4) | NibbleFromHex(hexIt[1])));
  }
  return bytes;
}

}
