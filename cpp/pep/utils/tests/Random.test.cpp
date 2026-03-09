#include <gtest/gtest.h>

#include <pep/utils/Random.hpp>

#include <algorithm>
#include <functional>

namespace {

TEST(RandomBytes, zeros) {
  std::array<std::byte, sizeof(std::uint64_t) * 150> buf{};
  pep::RandomBytes(buf);
  auto numZeros = std::ranges::count(buf, std::byte{});
  EXPECT_LT(numZeros, buf.size() / sizeof(std::uint64_t))
    << "Found suspiciously many zeros in random buffer of " << buf.size() << " bytes";
}

TEST(RandomBytes, tailZeros) {
  std::array<bool, sizeof(std::uint64_t) - 1> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    std::array<std::byte, nonzeroSeen.size()> buf{};
    pep::RandomBytes(buf);
    for (std::size_t i{}; i < buf.size(); ++i) {
      nonzeroSeen[i] |= buf[i] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes of tail in random buffer are always zero";
}

}
