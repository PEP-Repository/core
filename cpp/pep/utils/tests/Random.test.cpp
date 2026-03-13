#include <gtest/gtest.h>

#include <pep/utils/Random.hpp>

#include <algorithm>
#include <functional>
#include <type_traits>

namespace {

TEST(RandomBytes, zerosFraction) {
  for (const auto size : {512u, 500u, 500u /*repeated query*/, 1000u}) {
    std::vector<std::byte> buf(size);
    pep::RandomBytes(buf);
    auto numZeros = std::ranges::count(buf, std::byte{});
    EXPECT_LT(numZeros, buf.size() / sizeof(std::uint64_t))
      << "Found suspiciously many zeros in random buffer: " << numZeros << '/' << buf.size() << " bytes";
  }
}

}
