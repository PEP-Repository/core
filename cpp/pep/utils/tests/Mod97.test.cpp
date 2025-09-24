#include <gtest/gtest.h>

#include <pep/utils/Mod97.hpp>
#include <stdexcept>

namespace {

TEST(Mod97Test, TestComputeCheckDigit) {
  // Values taken from https://usersite.datalab.eu/printclass.aspx?type=wiki&id=91772
  EXPECT_EQ(pep::Mod97::ComputeCheckDigits("0600001234567"), "58");
  EXPECT_EQ(pep::Mod97::ComputeCheckDigits("0600001234586"), "98");

  // Some random short pseudonyms
  EXPECT_EQ(pep::Mod97::ComputeCheckDigits("POM-TEST-12345"), "46");
  EXPECT_EQ(pep::Mod97::ComputeCheckDigits("POM-TEST-12354"), "19");
  EXPECT_EQ(pep::Mod97::ComputeCheckDigits("POM-TSET-12345"), "64");

  EXPECT_THROW(pep::Mod97::ComputeCheckDigits(""), std::out_of_range);
  EXPECT_THROW(pep::Mod97::ComputeCheckDigits("a"), std::out_of_range);
}

TEST(Mod97Test, TestVerify) {
  // Values taken from https://usersite.datalab.eu/printclass.aspx?type=wiki&id=91772
  EXPECT_TRUE(pep::Mod97::Verify("060000123456758"));
  EXPECT_TRUE(pep::Mod97::Verify("060000123458698"));

  // Some random short pseudonyms
  EXPECT_TRUE(pep::Mod97::Verify("POM-TEST-12345-46"));
  EXPECT_TRUE(pep::Mod97::Verify("POM-TEST-12354-19"));
  EXPECT_TRUE(pep::Mod97::Verify("POM-TSET-12345-64"));

  EXPECT_FALSE(pep::Mod97::Verify("POM-TEST-12345-00"));

  EXPECT_FALSE(pep::Mod97::Verify(""));
  EXPECT_FALSE(pep::Mod97::Verify("a"));
  EXPECT_FALSE(pep::Mod97::Verify("ab"));
  EXPECT_FALSE(pep::Mod97::Verify("ab-00"));
}

}
