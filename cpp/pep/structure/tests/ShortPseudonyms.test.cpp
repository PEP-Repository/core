#include <gtest/gtest.h>

#include <pep/structure/ShortPseudonyms.hpp>

namespace {

TEST(ShortPseudonymsTest, TestGenerateShortPseudonym) {
  const std::string prefix = "POM-TEST-";
  std::string shortPseudonym = pep::GenerateShortPseudonym(prefix, 5);
  EXPECT_NE(shortPseudonym, "");
  EXPECT_TRUE(shortPseudonym.starts_with(prefix));
  EXPECT_EQ(shortPseudonym.size(), prefix.size() + 5 + 2);
  EXPECT_TRUE(pep::ShortPseudonymIsValid(shortPseudonym));
}

TEST(ShortPseudonymsTest, TestIsValid) {
  // Some random short pseudonyms
  EXPECT_TRUE(pep::ShortPseudonymIsValid("POM-TEST-25"));
  EXPECT_TRUE(pep::ShortPseudonymIsValid("POM-TEST-12345-46"));
  EXPECT_TRUE(pep::ShortPseudonymIsValid("POM-TEST-12354-19"));
  EXPECT_TRUE(pep::ShortPseudonymIsValid("POM-TSET-12345-64"));

  EXPECT_FALSE(pep::ShortPseudonymIsValid(""));
  EXPECT_FALSE(pep::ShortPseudonymIsValid("POM-TEST-12345-00"));
  EXPECT_FALSE(pep::ShortPseudonymIsValid("POM-TEST-10345-46"));
}

}
