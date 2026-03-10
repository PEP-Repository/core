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

TEST(RandomBytes, smallZeros) {
  std::array<bool, sizeof(std::uint64_t) - 1> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    // Layout in aligned blocks: BBBBBBB_
    alignas(std::uint64_t) std::array<std::byte, nonzeroSeen.size()> buf{};
    pep::RandomBytes(buf);
    for (std::size_t i{}; i < nonzeroSeen.size(); ++i) {
      nonzeroSeen[i] |= buf[i] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes in a small random buffer are always zero";
}

TEST(RandomBytes, smallUnalignedZeros) {
  std::array<bool, sizeof(std::uint64_t) * 2 - 2> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    alignas(sizeof(std::uint64_t)) std::array<std::byte, 1 + nonzeroSeen.size()> aligned{};
    // This is larger than a uint64, but because of alignment,
    // RandomBytes may only be allowed to write bytes.
    // Layout in aligned blocks: _BBBBBBB BBBBBBB_
    auto unaligned = std::span(aligned).subspan(1);
    pep::RandomBytes(unaligned);
    for (std::size_t i{}; i < nonzeroSeen.size(); ++i) {
      nonzeroSeen[i] |= unaligned[i] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes in a small unaligned random buffer are always zero";
}

TEST(RandomBytes, endZeros) {
  std::array<bool, sizeof(std::uint64_t) - 1> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    // Layout in aligned blocks: BBBBBBBB BBBBBBBB BBBBBBB_
    alignas(std::uint64_t) std::array<std::byte, nonzeroSeen.size() + 2 * sizeof(std::uint64_t)> buf{};
    pep::RandomBytes(buf);
    for (std::size_t i{}; i < nonzeroSeen.size(); ++i) {
      nonzeroSeen[i] |= buf[i + (buf.size() - nonzeroSeen.size())] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes at the end of a random buffer are always zero";
}

TEST(RandomBytes, unalignedEndZeros) {
  std::array<bool, sizeof(std::uint64_t) - 1> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    alignas(sizeof(std::uint64_t)) std::array<std::byte, 1 + 2 * sizeof(std::uint64_t)> aligned{};
    // Layout in aligned blocks: _BBBBBBB BBBBBBBB B_______
    auto unaligned = std::span(aligned).subspan(1);
    pep::RandomBytes(unaligned);
    for (std::size_t i{}; i < nonzeroSeen.size(); ++i) {
      nonzeroSeen[i] |= unaligned[i + (unaligned.size() - nonzeroSeen.size())] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes at the end of an unaligned random buffer are always zero";
}

TEST(RandomBytes, unalignedStartZeros) {
  std::array<bool, sizeof(std::uint64_t) - 1> nonzeroSeen{};
  for (unsigned iteration{}; iteration < 150; ++iteration) {
    alignas(sizeof(std::uint64_t)) std::array<std::byte, 1 + 2 * sizeof(std::uint64_t)> aligned{};
    // Layout in aligned blocks: _BBBBBBB BBBBBBBB B_______
    auto unaligned = std::span(aligned).subspan(1);
    pep::RandomBytes(unaligned);
    for (std::size_t i{}; i < nonzeroSeen.size(); ++i) {
      nonzeroSeen[i] |= unaligned[i] != std::byte{};
    }
  }

  std::array<bool, nonzeroSeen.size()> allTrue{};
  std::ranges::fill(allTrue, true);
  EXPECT_EQ(nonzeroSeen, allTrue) << "Some bytes at the start of an unaligned random buffer are always zero";
}

}
