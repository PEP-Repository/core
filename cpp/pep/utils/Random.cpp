#include <pep/utils/Random.hpp>

#include <pep/utils/Math.hpp>
#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>

#include <cassert>
#include <memory>
#include <random>

namespace pep {

namespace {
void AssignLeftoverRandomBytes(std::span<std::byte> outLeftover) {
  if (!outLeftover.empty()) {
    assert(outLeftover.size() < sizeof(typename CryptoUrbg::result_type));
    auto finalNum = static_cast<std::make_unsigned_t<CryptoUrbg::result_type>>(ThreadUrbg());
    for (auto& to : outLeftover) {
      to = static_cast<std::byte>(finalNum & 0xff);
      finalNum >>= 8;
    }
  }
}
}

void UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  if (RAND_bytes(outInts.data(), CheckedCast<int>(outInts.size())) <= 0) {
    throw OpenSSLError("RAND_bytes failed");
  }
}

static_assert(std::uniform_random_bit_generator<CryptoUrbg>);

void RandomBytes(std::span<std::byte> out) {
  // Confirm that the URBG generates the full range
  static_assert(
    CryptoUrbg::min() == std::numeric_limits<CryptoUrbg::result_type>::min()
    && CryptoUrbg::max() == std::numeric_limits<CryptoUrbg::result_type>::max());

  // First try to assign values as returned from the URBG.
  // We align the pointer if necessary for a multi-byte data type,
  // because not all platforms support unaligned writes to larger types.
  void* alignPtr = out.data();
  std::size_t spaceExclAlign = out.size();
  static_assert(alignof(CryptoUrbg::result_type) <= sizeof(CryptoUrbg::result_type));
  if constexpr (alignof(CryptoUrbg::result_type) > 1) {
    (void) std::align(alignof(CryptoUrbg::result_type), 0, alignPtr, spaceExclAlign);
  }
  std::span outAlignedWords(
    static_cast<CryptoUrbg::result_type*>(alignPtr),
    spaceExclAlign / sizeof(typename CryptoUrbg::result_type));
  assert(outAlignedWords.size_bytes() <= out.size());
  //XXX Replace by std::generate_random in C++26
  std::ranges::generate(outAlignedWords, std::ref(ThreadUrbg));

  [[maybe_unused]] auto outAlignedBytes = std::as_writable_bytes(outAlignedWords);
  if constexpr (alignof(CryptoUrbg::result_type) > 1) {
    // Assign start: left of outAlignedWords
    AssignLeftoverRandomBytes(std::span(out.data(), outAlignedBytes.data()));
  }
  if constexpr (sizeof(CryptoUrbg::result_type) > 1) {
    // Assign end: right of outAlignedWords
    AssignLeftoverRandomBytes(std::span(outAlignedBytes.data() + outAlignedBytes.size(), out.data() + out.size()));
  }
}

thread_local CryptoUrbg ThreadUrbg;
}
