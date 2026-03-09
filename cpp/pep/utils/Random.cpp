#include <pep/utils/Random.hpp>

#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>

#include <cassert>
#include <random>

void pep::UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  if (RAND_bytes(outInts.data(), static_cast<int>(outInts.size())) <= 0) {
    throw pep::OpenSSLError("RAND_bytes failed");
  }
}

static_assert(std::uniform_random_bit_generator<pep::SecureUrbg>);

void pep::RandomBytes(std::span<std::byte> out) {
  static_assert(
    SecureUrbg::min() == std::numeric_limits<SecureUrbg::result_type>::min()
    && SecureUrbg::max() == std::numeric_limits<SecureUrbg::result_type>::max());

  std::span outLarge(
    reinterpret_cast<SecureUrbg::result_type*>(out.data()),
    out.size() / sizeof(typename SecureUrbg::result_type));
  assert(outLarge.size_bytes() <= out.size());
  //XXX Replace by std::generate_random in C++26
  std::ranges::generate(outLarge, std::ref(ThreadUrbg));

  if constexpr (sizeof(typename SecureUrbg::result_type) > 1) {
    auto outLeftover = out.subspan(outLarge.size_bytes());
    if (!outLeftover.empty()) {
      assert(outLeftover.size() < sizeof(typename SecureUrbg::result_type));
      auto finalNum = static_cast<std::make_unsigned_t<SecureUrbg::result_type>>(ThreadUrbg());
      for (auto& to : outLeftover) {
        to = static_cast<std::byte>(finalNum & 0xff);
        finalNum >>= 8;
      }
    }
  }
}

thread_local pep::SecureUrbg pep::ThreadUrbg;
