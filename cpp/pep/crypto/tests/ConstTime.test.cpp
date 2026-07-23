#include <pep/crypto/ConstTime.hpp>

#include <pep/utils/TestError.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>

#include <gtest/gtest.h>

using namespace std::literals;
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

TEST(ConstTime, ToHex) {
  EXPECT_EQ(pep::const_time::ToHex("abc"), "616263");
  EXPECT_EQ(pep::const_time::ToHex("\x00\x01\x02\x89\xaa\xab\xff"sv), "00010289AAABFF");
  EXPECT_EQ(pep::const_time::ToHex(""), "");  // edge case
}

TEST(ConstTime, FromHex) {
  EXPECT_EQ(pep::const_time::FromHex("616263"), "abc");
  EXPECT_EQ(pep::const_time::FromHex("00010289AAABFF"), "\x00\x01\x02\x89\xaa\xab\xff"sv);
  EXPECT_EQ(pep::const_time::FromHex("aa"), "\xaa") << "FromHex should support lowercase";
  EXPECT_EQ(pep::const_time::FromHex(""), "");  // edge case

  EXPECT_THROW(pep::const_time::FromHex("A"), std::invalid_argument)
    << "FromHex should reject strings with a length that is not a multiple of 2";

  EXPECT_THROW(pep::const_time::FromHex("\0\0"sv), std::invalid_argument);
  EXPECT_THROW(pep::const_time::FromHex("//"), std::invalid_argument); // Before '0'
  EXPECT_THROW(pep::const_time::FromHex("::"), std::invalid_argument); // After '9'
  EXPECT_THROW(pep::const_time::FromHex("@@"), std::invalid_argument); // Before 'A'
  EXPECT_THROW(pep::const_time::FromHex("GG"), std::invalid_argument);
  EXPECT_THROW(pep::const_time::FromHex("[["), std::invalid_argument); // After 'Z'
  EXPECT_THROW(pep::const_time::FromHex("``"), std::invalid_argument); // Before 'a'
  EXPECT_THROW(pep::const_time::FromHex("gg"), std::invalid_argument);
  EXPECT_THROW(pep::const_time::FromHex("{{"), std::invalid_argument); // After 'z'
  EXPECT_THROW(pep::const_time::FromHex("\xff\xff"), std::invalid_argument);
}

}
