#include <pep/utils/Random.hpp>

#include <pep/utils/Math.hpp>
#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>
#include <cassert>
#include <cstring>

namespace pep {

namespace {

class RandomBytesBuffer {
public:
  static constexpr size_t capacity = 512;

private:
  std::array<std::byte, capacity> content_;
  size_t index_ = capacity;

  std::span<std::byte> bufferedData() {
    return std::span<std::byte>(content_).subspan(index_);
  }

  size_t fillFromBuffer(std::span<std::byte> destination) {
    auto source = this->bufferedData();
    auto result = std::min(source.size(), destination.size());

    CopyToRange(source, destination);
    index_ += result;

    // Zero-fill consumed randomness so that secrets don't remain in memory
    auto consumed = source.subspan(0U, result);
    std::ranges::fill(consumed.begin(), consumed.end(), std::byte{});

    return result;
  }

public:
  void fill(std::span<std::byte> destination) {
    assert(destination.size() <= capacity);

    // (Try to) fill destination from what we still had buffered.
    auto filled = this->fillFromBuffer(destination);

    // If we didn't have enough buffered data to fill destination entirely...
    auto remaining = destination.subspan(filled);
    if (remaining.size() != 0U) [[unlikely]] {
      // ... re-fill our buffer...
      UnbufferedRandomBytes(content_);
      index_ = 0U;

      // ... then fill remaining bytes
      filled = this->fillFromBuffer(remaining);
      assert(filled == remaining.size());
    }
  }
};

}

void UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  // Benchmarking indicates that OpenSSL function RAND_bytes has better performance (on some platforms) than std::random_device
  if (RAND_bytes(outInts.data(), CheckedCast<int>(outInts.size())) <= 0) {
    throw OpenSSLError("RAND_bytes failed");
  }
}

void RandomBytes(std::span<std::byte> out) {
  if (out.size() > RandomBytesBuffer::capacity) [[unlikely]] {
    // Fill directly if our buffer is smaller
    UnbufferedRandomBytes(out);
  }
  else {
    thread_local RandomBytesBuffer buffer;
    buffer.fill(out);
  }
}

static_assert(std::uniform_random_bit_generator<CryptoUrbg>);

}
