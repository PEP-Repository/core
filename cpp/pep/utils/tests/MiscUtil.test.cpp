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

TEST(MiscUtil, SiPrefix) {
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

  // Base-2 (binary) prefixes: 1kiB = 2^10 bytes, 1MiB = 2^20 bytes, and so on
  EXPECT_EQ(pep::SiPrefix(10U, 10U), "k");
  EXPECT_EQ(pep::SiPrefix(20U, 10U), "M");
  EXPECT_EQ(pep::SiPrefix(30U, 10U), "G");
  EXPECT_EQ(pep::SiPrefix(40U, 10U), "T");
  EXPECT_EQ(pep::SiPrefix(50U, 10U), "P");
  EXPECT_EQ(pep::SiPrefix(60U, 10U), "E");
  EXPECT_EQ(pep::SiPrefix(70U, 10U), "Z");
  EXPECT_EQ(pep::SiPrefix(80U, 10U), "Y");
  EXPECT_EQ(pep::SiPrefix(90U, 10U), "R");
  EXPECT_EQ(pep::SiPrefix(100U, 10U), "Q");
}

}
