#include <pep/crypto/ConstTime.hpp>

#include <pep/utils/TestError.hpp>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

using namespace std::ranges;

namespace {

constexpr auto LazyThrowingIota =
  views::iota(0, 10)
  | views::transform([](int i) {
    if (i > 5) {
      throw pep::TestError{};
    }
    return i;
  });

TEST(ConstTime, IsZero) {
  EXPECT_TRUE(pep::const_time::IsZero(std::array{0, 0, 0}));
  EXPECT_FALSE(pep::const_time::IsZero(std::array{0, 0b11, 0}));

  // Make sure IsZero does not use short-circuit logic
  EXPECT_THROW((void) pep::const_time::IsZero(LazyThrowingIota), pep::TestError);
  // STL for comparison
  EXPECT_NO_THROW((void) all_of(LazyThrowingIota, std::bind_front(std::equal_to{}, 0)));
}

TEST(ConstTime, IsEqual) {
  EXPECT_TRUE(pep::const_time::IsEqual(std::array{0, 0, 0}, std::array{0, 0, 0}));
  EXPECT_TRUE(pep::const_time::IsEqual(std::array{0, 0b11, 0}, std::array{0, 0b11, 0}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 0b00, 0}, std::array{0, 0b11, 0}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 0b01, 0}, std::array{0, 0b10, 0}));

  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 1, 2}, std::array{0, 1}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 1}, std::array{0, 1, 2}));

  // Make sure IsEqual does not use short-circuit logic
  EXPECT_THROW((void) pep::const_time::IsEqual(LazyThrowingIota, views::iota(10, 20)), pep::TestError);
  // STL for comparison
  EXPECT_NO_THROW((void) equal(LazyThrowingIota, views::iota(10, 20)));
}

}
