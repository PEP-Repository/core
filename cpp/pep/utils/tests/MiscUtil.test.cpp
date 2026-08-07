#include <pep/utils/MiscUtil.hpp>
#include <gtest/gtest.h>

namespace {

TEST(MiscUtil, BoolToString) {
  ASSERT_EQ(pep::BoolToString(true), "true");
  ASSERT_EQ(pep::BoolToString(false), "false");
}

TEST(MiscUtil, StringToBool) {
  ASSERT_EQ(pep::StringToBool("true"), true);
  ASSERT_EQ(pep::StringToBool("1"), false);
  ASSERT_EQ(pep::StringToBool("foobar"), false);
  ASSERT_EQ(pep::StringToBool("false"), false);
}

TEST(MiscUtil, UnitPrefix) {
  EXPECT_EQ(pep::SiPrefix(1), "da");
  EXPECT_EQ(pep::SiPrefix(2), "h");
  EXPECT_EQ(pep::SiPrefix(3), "k");
  EXPECT_EQ(pep::SiPrefix(6), "M");
  EXPECT_EQ(pep::SiPrefix(9), "G");
  EXPECT_EQ(pep::SiPrefix(12), "T");
  EXPECT_EQ(pep::SiPrefix(15), "P");
  EXPECT_EQ(pep::SiPrefix(18), "E");
  EXPECT_EQ(pep::SiPrefix(21), "Z");
  EXPECT_EQ(pep::SiPrefix(24), "Y");
  EXPECT_EQ(pep::SiPrefix(27), "R");
  EXPECT_EQ(pep::SiPrefix(30), "Q");

  EXPECT_EQ(pep::SiPrefix(-1), "d");
  EXPECT_EQ(pep::SiPrefix(-2), "c");
  EXPECT_EQ(pep::SiPrefix(-3), "m");
  EXPECT_EQ(pep::SiPrefix(-6), pep::MicroSymbol);
  EXPECT_EQ(pep::SiPrefix(-9), "n");
  EXPECT_EQ(pep::SiPrefix(-12), "p");
  EXPECT_EQ(pep::SiPrefix(-15), "f");
  EXPECT_EQ(pep::SiPrefix(-18), "a");
  EXPECT_EQ(pep::SiPrefix(-21), "z");
  EXPECT_EQ(pep::SiPrefix(-24), "y");
  EXPECT_EQ(pep::SiPrefix(-27), "r");
  EXPECT_EQ(pep::SiPrefix(-30), "q");

  EXPECT_EQ(pep::SiPrefix(-33), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(-4), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(0), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(16), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(33), std::nullopt);

  // Binary (base-2) prefixes: 1kiB = 2^10 bytes, 1MiB = 2^20 bytes, and so on
  EXPECT_EQ(pep::BinaryPrefix(10U), "ki");
  EXPECT_EQ(pep::BinaryPrefix(20U), "Mi");
  EXPECT_EQ(pep::BinaryPrefix(30U), "Gi");
  EXPECT_EQ(pep::BinaryPrefix(40U), "Ti");
  EXPECT_EQ(pep::BinaryPrefix(50U), "Pi");
  EXPECT_EQ(pep::BinaryPrefix(60U), "Ei");
  EXPECT_EQ(pep::BinaryPrefix(70U), "Zi");
  EXPECT_EQ(pep::BinaryPrefix(80U), "Yi");
  EXPECT_EQ(pep::BinaryPrefix(90U), "Ri");
  EXPECT_EQ(pep::BinaryPrefix(100U), "Qi");
}

}
