#include <gtest/gtest.h>

#include <pep/utils/Base64.hpp>

namespace {

// Test vectors from RFC 4648
TEST(Base64Test, TestURLEncoding) {
  EXPECT_EQ(pep::EncodeBase64Url(""), "");
  EXPECT_EQ(pep::EncodeBase64Url("f"), "Zg");
  EXPECT_EQ(pep::EncodeBase64Url("fo"), "Zm8");
  EXPECT_EQ(pep::EncodeBase64Url("foo"), "Zm9v");
  EXPECT_EQ(pep::EncodeBase64Url("foob"), "Zm9vYg");
  EXPECT_EQ(pep::EncodeBase64Url("fooba"), "Zm9vYmE");
  EXPECT_EQ(pep::EncodeBase64Url("foobar"), "Zm9vYmFy");
}

// Test vectors from RFC 4648
TEST(Base64Test, TestURLDecoding) {
  EXPECT_EQ(pep::DecodeBase64Url(""), "");
  EXPECT_EQ(pep::DecodeBase64Url("Zg"), "f");
  EXPECT_EQ(pep::DecodeBase64Url("Zm8"), "fo");
  EXPECT_EQ(pep::DecodeBase64Url("Zm9v"), "foo");
  EXPECT_EQ(pep::DecodeBase64Url("Zm9vYg"), "foob");
  EXPECT_EQ(pep::DecodeBase64Url("Zm9vYmE"), "fooba");
  EXPECT_EQ(pep::DecodeBase64Url("Zm9vYmFy"), "foobar");
}

}
