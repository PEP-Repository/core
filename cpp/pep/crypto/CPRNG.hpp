#pragma once

#include <array>
#include <string>
#include <memory>
#include <limits>
#include <mutex>
#include <span>

#include <boost/core/noncopyable.hpp>

#include <openssl/evp.h>

#include <pep/utils/Random.hpp>

namespace pep {

using EvpContext = std::unique_ptr<EVP_CIPHER_CTX, void(*)(EVP_CIPHER_CTX*)>;

// A cryptographically secure pseudo-random number generator.  An instance
// of CPRNG is seeded with 384 bits of entropy using RandomBytes().
//
// To generate keys, use RandomBytes() itself.
// To std::shuffle a vector, use a new instance of CPURBG instead.
// To generate many random CurvePoints for a single request, a new instance
// of CPRNG is great.
//
// CPRNG is thread-safe
class CPRNG : private boost::noncopyable {
  EvpContext mCtx;
  std::mutex mLock; // TODO use thread-local storage instead of lock

 public:
  CPRNG();
  void operator()(uint8_t* buffer, size_t len) { return (*this)({buffer, len}); }
  void operator()(std::span<uint8_t> buffer);
};

// A cryptographically secure pseudo-random number generator compatible
// with std::random_device. (An "URBG".)  An instance of CPURBG is
// seeded with 384 bits of entropy using RandomBytes()
//
// To generate keys, use RandomBytes() itself.
// To std::shuffle a vector, a new instance of CPURBG is great.
//
// CPURBG is *not* thread-safe
class CPURBG : private boost::noncopyable {
 private:
  static constexpr int bufferSize = 16;
  EvpContext mCtx;
  std::array<uint64_t, bufferSize> mBuffer{};
  uint64_t mFillCount = 0;
  int mBufferIdx = bufferSize;
  void fillBuffer();

 public:
  using result_type = uint64_t;

  CPURBG();
  result_type operator()() {
    if (mBufferIdx == bufferSize)
      fillBuffer();
    return mBuffer[static_cast<size_t>(mBufferIdx++)];
  }

  static constexpr result_type min() {
    return 0;
  }

  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }
};

}
