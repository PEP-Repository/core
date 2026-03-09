#include <pep/utils/Random.hpp>

#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>

#include <cassert>
#include <memory>
#include <random>

void pep::UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  if (RAND_bytes(outInts.data(), static_cast<int>(outInts.size())) <= 0) {
    throw pep::OpenSSLError("RAND_bytes failed");
  }
}

static_assert(std::uniform_random_bit_generator<pep::SecureUrbg>);

static void AssignLeftoverRandomBytes(std::span<std::byte> outLeftover) {
  if (!outLeftover.empty()) {
    assert(outLeftover.size() < sizeof(typename pep::SecureUrbg::result_type));
    auto finalNum = static_cast<std::make_unsigned_t<pep::SecureUrbg::result_type>>(pep::ThreadUrbg());
    for (auto& to : outLeftover) {
      to = static_cast<std::byte>(finalNum & 0xff);
      finalNum >>= 8;
    }
  }
}

void pep::RandomBytes(std::span<std::byte> out) {
  // Confirm that the URBG generates the full range
  static_assert(
    SecureUrbg::min() == std::numeric_limits<SecureUrbg::result_type>::min()
    && SecureUrbg::max() == std::numeric_limits<SecureUrbg::result_type>::max());

  // First try to assign values as returned from the URBG.
  // We align the pointer if necessary for a multi-byte data type.
  void* alignPtr = out.data();
  std::size_t spaceExclAlign = out.size();
  static_assert(alignof(SecureUrbg::result_type) <= sizeof(SecureUrbg::result_type));
  if constexpr (alignof(SecureUrbg::result_type) > 1) {
    (void) std::align(alignof(SecureUrbg::result_type), 0, alignPtr, spaceExclAlign);
  }
  std::span outAligned(
    static_cast<SecureUrbg::result_type*>(alignPtr),
    spaceExclAlign / sizeof(typename SecureUrbg::result_type));
  assert(outAligned.size_bytes() <= out.size());
  //XXX Replace by std::generate_random in C++26
  std::ranges::generate(outAligned, std::ref(ThreadUrbg));

  [[maybe_unused]] auto outAlignedBytes = std::as_writable_bytes(outAligned);
  if constexpr (alignof(SecureUrbg::result_type) > 1) {
    // Assign start
    AssignLeftoverRandomBytes(std::span(out.data(), outAlignedBytes.data()));
  }
  if constexpr (sizeof(SecureUrbg::result_type) > 1) {
    // Assign end
    AssignLeftoverRandomBytes(std::span(outAlignedBytes.data() + outAlignedBytes.size(), out.data() + out.size()));
  }
}

thread_local pep::SecureUrbg pep::ThreadUrbg;
