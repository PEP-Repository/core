#pragma once

#include <pep/utils/CollectionUtils.hpp>

#include <boost/noncopyable.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace pep {

void UnbufferedRandomBytes(std::span<std::byte> out);

/// A cryptographically secure pseudo-random number generator satisfying \c std::uniform_random_bit_generator
/// \warning Not thread-safe
class CryptoUrbg : boost::noncopyable {
public:
  // 64-bit is much faster than 8-bit
  using result_type = std::uint64_t;

private:
  // Larger calls to UnbufferedRandomBytes greatly increase speed, see benchmark.cpp
  std::array<result_type, 512 / sizeof(result_type)> buffer_{};
  unsigned bufferOffset_ = static_cast<decltype(bufferOffset_)>(buffer_.size());

public:
  // Define inline for performance
  [[nodiscard]] result_type operator()() {
    if (bufferOffset_ == buffer_.size()) {
      UnbufferedRandomBytes(std::as_writable_bytes(std::span{buffer_}));
      bufferOffset_ = 0;
    }
    // Also zero-out data such that secrets don't remain here in memory
    return std::exchange(buffer_[bufferOffset_++], 0);
  }

  [[nodiscard]] static constexpr result_type min() noexcept {
    return std::numeric_limits<result_type>::min();
  }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
};

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
  // Try to align contents for efficiency
  alignas(CryptoUrbg::result_type) std::array<std::byte, Length> buf; //NOLINT(cppcoreguidelines-pro-type-member-init)
  RandomBytes(buf);
  return buf;
}

extern thread_local CryptoUrbg ThreadUrbg;

}
