#include <pep/crypto/ConstTime.hpp>

#include <algorithm>

#include <gtest/gtest.h>

using namespace std::ranges;

namespace {

//TODO Use class from https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/2211/diffs?commit_id=2ed021e4e20832b74aa4fd72ea8e7ba98f10b2d2#fa2a04b1b3dba5ead10100f492690a0b1a03c6c4
struct TestError : std::exception {
  [[nodiscard]] const char* what() const noexcept override { return "TestError"; }
};

constexpr auto LazyThrowingIota =
  views::iota(0, 10)
  | views::transform([](int i) {
    if (i > 5) {
      throw TestError{};
    }
    return i;
  });

TEST(ConstTime, IsZero) {
  EXPECT_TRUE(pep::const_time::IsZero(std::array{0, 0, 0}));
  EXPECT_FALSE(pep::const_time::IsZero(std::array{0, 0b11, 0}));

  // Make sure IsZero does not use short-circuit logic
  EXPECT_THROW(pep::const_time::IsZero(LazyThrowingIota), TestError);
  // STL for comparison
  EXPECT_NO_THROW(all_of(LazyThrowingIota, std::bind_front(std::equal_to{}, 0)));
}

TEST(ConstTime, IsEqual) {
  EXPECT_TRUE(pep::const_time::IsEqual(std::array{0, 0, 0}, std::array{0, 0, 0}));
  EXPECT_TRUE(pep::const_time::IsEqual(std::array{0, 0b11, 0}, std::array{0, 0b11, 0}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 0b00, 0}, std::array{0, 0b11, 0}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 0b01, 0}, std::array{0, 0b10, 0}));

  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 1, 2}, std::array{0, 1}));
  EXPECT_FALSE(pep::const_time::IsEqual(std::array{0, 1}, std::array{0, 1, 2}));

  // Make sure IsEqual does not use short-circuit logic
  EXPECT_THROW(pep::const_time::IsEqual(LazyThrowingIota, views::iota(10, 20)), TestError);
  // STL for comparison
  EXPECT_NO_THROW(equal(LazyThrowingIota, views::iota(10, 20)));
}

}
