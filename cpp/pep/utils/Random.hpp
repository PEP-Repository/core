#pragma once

#include <pep/utils/CollectionUtils.hpp>

#include <boost/noncopyable.hpp>

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

/// A cryptographically secure pseudo-random number generator satisfying \c std::uniform_random_bit_generator
/// \warning Not thread-safe
class CryptoUrbg : boost::noncopyable {
public:
  using result_type = unsigned char;

private:
  // Larger calls to UnbufferedRandomBytes greatly increase speed, see benchmark.cpp
  std::array<result_type, 512 / sizeof(result_type)> buffer_{};
  unsigned bufferOffset_ = static_cast<decltype(bufferOffset_)>(buffer_.size());

  auto assignFromBuffer(std::span<result_type> out) {
    const auto remainingBuffer = std::span{buffer_}.subspan(bufferOffset_);
    const auto res = CopyToRange( remainingBuffer, out);
    // Try to zero-out data such that secrets don't remain here in memory
    std::ranges::fill(remainingBuffer.begin(), res.in, result_type{});
    const auto numRead = res.in - remainingBuffer.begin();
    bufferOffset_ += static_cast<decltype(bufferOffset_)>(numRead);
    return res.out;
  }

  void fillBuffer() {
    UnbufferedRandomBytes(std::as_writable_bytes(std::span{buffer_}));
    bufferOffset_ = 0;
  }

public:
  // Define inline for performance
  [[nodiscard]] result_type operator()() {
    if (bufferOffset_ == buffer_.size()) [[unlikely]] {
      fillBuffer();
    }
    // Also zero-out data such that secrets don't remain here in memory
    return std::exchange(buffer_[bufferOffset_++], 0);
  }

  // C++26-compatible interface
  void generate_random(std::span<result_type> out) {
    if (out.size() >= buffer_.size()) [[unlikely]] {
      // Fill directly if our buffer is smaller
      UnbufferedRandomBytes(std::as_writable_bytes(out));
    } else {
      // First copy what's left in the buffer
      const std::span remainingOut(assignFromBuffer(out), out.end());
      if (!remainingOut.empty()) {
        // If that wasn't enough, assign from a new buffer
        fillBuffer();
        assignFromBuffer(remainingOut);
      }
    }
  }

  [[nodiscard]] static constexpr result_type min() noexcept {
    return std::numeric_limits<result_type>::min();
  }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
};
extern thread_local CryptoUrbg ThreadUrbg;

/// Generate secure random numbers
inline void RandomBytes(std::span<std::byte> out) {
  ThreadUrbg.generate_random(ConvertBytes<unsigned char>(out));
}
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

}
