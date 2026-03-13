#include <pep/utils/Random.hpp>

#include <pep/utils/Math.hpp>
#include <pep/utils/OpensslUtils.hpp>

#include <openssl/rand.h>

#include <cassert>
#include <memory>
#include <random>

namespace pep {

void UnbufferedRandomBytes(std::span<std::byte> out) {
  auto outInts = ConvertBytes<unsigned char>(out);
  if (RAND_bytes(outInts.data(), CheckedCast<int>(outInts.size())) <= 0) {
    throw OpenSSLError("RAND_bytes failed");
  }
}

static_assert(std::uniform_random_bit_generator<CryptoUrbg>);

thread_local CryptoUrbg ThreadUrbg;
}
