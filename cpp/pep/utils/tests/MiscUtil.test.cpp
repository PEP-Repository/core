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
  EXPECT_EQ(pep::SiPrefix(3U), "k");
  EXPECT_EQ(pep::SiPrefix(6U), "M");
  EXPECT_EQ(pep::SiPrefix(9U), "G");
  EXPECT_EQ(pep::SiPrefix(12U), "T");
  EXPECT_EQ(pep::SiPrefix(15U), "P");
  EXPECT_EQ(pep::SiPrefix(18U), "E");
  EXPECT_EQ(pep::SiPrefix(21U), "Z");
  EXPECT_EQ(pep::SiPrefix(24U), "Y");
  EXPECT_EQ(pep::SiPrefix(27U), "R");
  EXPECT_EQ(pep::SiPrefix(30U), "Q");

  EXPECT_EQ(pep::SiPrefix(0U), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(1U), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(2U), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(14U), std::nullopt);
  EXPECT_EQ(pep::SiPrefix(33U), std::nullopt);

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
