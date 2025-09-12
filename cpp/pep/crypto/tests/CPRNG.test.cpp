#include <gtest/gtest.h>
#include <pep/crypto/CPRNG.hpp>

#include <algorithm>
#include <vector>

namespace {

TEST(CPRNG, NonZero) {
  std::vector<uint8_t> buf(100'000);
  const auto allZero = [&buf] {
    return std::ranges::all_of(buf, [](uint8_t b) { return b == 0; });
  };
  pep::CPRNG rng;
  rng(buf.data(), 100);
  EXPECT_FALSE(allZero()) << "Buffer of zeros found to be equal to randomly filled buffer. Size = 100 bytes";
  rng(buf.data(), 1'000);
  EXPECT_FALSE(allZero()) << "Buffer of zeros found to be equal to randomly filled buffer. Size = 1000 bytes";
  rng(buf.data(), 10'000);
  EXPECT_FALSE(allZero()) << "Buffer of zeros found to be equal to randomly filled buffer. Size = 10000 bytes";
  rng(buf.data(), 100'000);
  EXPECT_FALSE(allZero()) << "Buffer of zeros found to be equal to randomly filled buffer. Size = 100000 bytes";
}

}
