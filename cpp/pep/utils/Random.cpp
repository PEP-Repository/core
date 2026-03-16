#include <pep/utils/Random.hpp>

#include <pep/utils/Math.hpp>
#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>
#include <cstring>

namespace pep {

namespace {

class RandomBytesBuffer {
public:
  static constexpr size_t capacity = 512;

private:
  std::array<std::byte, capacity> content_;
  size_t index_ = capacity;

  size_t fillFromBuffer(std::byte* destination, size_t max) {
    auto available = capacity - index_;
    auto result = std::min(available, max);

    auto source = content_.data() + index_;
    std::memcpy(destination, source, result);

    std::memset(source, 0, result); // Zero-fill consumed randomness so that secrets don't remain in memory
    index_ += result;

    return result;
  }

public:
  void fill(std::byte* destination, size_t size) {
    assert(size <= capacity);

    // (Try to) fill destination from what we still had buffered.
    auto filled = this->fillFromBuffer(destination, size);

    if (filled != size) [[unlikely]] { // If we didn't have sufficient data buffered...

      // ... re-fill our buffer...
      UnbufferedRandomBytes(content_);
      index_ = 0U;

      // ... then fill remaining bytes in "destination"
      filled += this->fillFromBuffer(destination + filled, size - filled);
      assert(filled == size);
    }
  }
};

}

void UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  if (RAND_bytes(outInts.data(), CheckedCast<int>(outInts.size())) <= 0) {
    throw OpenSSLError("RAND_bytes failed");
  }
}

void RandomBytes(std::span<std::byte> out) {
  auto size = out.size();
  if (size > RandomBytesBuffer::capacity) [[unlikely]] {
    // Fill directly if our buffer is smaller
    UnbufferedRandomBytes(out);
  }
  else {
    thread_local RandomBytesBuffer buffer;
    buffer.fill(out.data(), size);
  }
}

static_assert(std::uniform_random_bit_generator<CryptoUrbg>);

}
