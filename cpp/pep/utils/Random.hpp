#pragma once

#include <pep/utils/CollectionUtils.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <utility>

namespace pep {

void UnbufferedRandomBytes(std::span<std::byte> out);

/// Generate secure random numbers
void RandomBytes(std::span<std::byte> out);

inline void RandomBytes(std::span<char> out) { RandomBytes(std::as_writable_bytes(out)); }
inline void RandomBytes(std::span<unsigned char> out) { RandomBytes(std::as_writable_bytes(out)); }

[[nodiscard]] inline std::string RandomString(std::size_t len) {
  std::string result(len, '\0');
  RandomBytes(result);
  return result;
}

template <ByteLike Byte = std::byte>
[[nodiscard]] std::vector<Byte> RandomVector(std::size_t len) {
  std::vector<Byte> result(len);
  RandomBytes(result);
  return result;
}

template<std::size_t Length>
[[nodiscard]] std::array<std::byte, Length> RandomArray() {
  std::array<std::byte, Length> buf; //NOLINT(cppcoreguidelines-pro-type-member-init)
  RandomBytes(buf);
  return buf;
}

/// A cryptographically secure pseudo-random number generator satisfying \c std::uniform_random_bit_generator
/// \remark Thread-safe
struct CryptoUrbg {
  using result_type = std::uint64_t;

  // Define inline for performance
  // C++26-compatible interface
  void generate_random(std::span<result_type> out) {
    RandomBytes(std::as_writable_bytes(out));
  }

  [[nodiscard]] result_type operator()() {
    result_type result{};
    this->generate_random(std::span<result_type>(&result, 1U));
    return result;
  }

  [[nodiscard]] static constexpr result_type min() noexcept {
    return std::numeric_limits<result_type>::min();
  }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
};

}
