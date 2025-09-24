#include <gtest/gtest.h>

#include <pep/utils/Base64.hpp>

namespace {

// Test vectors from RFC 4648
TEST(Base64Test, TestURLEncoding) {
  EXPECT_EQ(pep::encodeBase64URL(""), "");
  EXPECT_EQ(pep::encodeBase64URL("f"), "Zg");
  EXPECT_EQ(pep::encodeBase64URL("fo"), "Zm8");
  EXPECT_EQ(pep::encodeBase64URL("foo"), "Zm9v");
  EXPECT_EQ(pep::encodeBase64URL("foob"), "Zm9vYg");
  EXPECT_EQ(pep::encodeBase64URL("fooba"), "Zm9vYmE");
  EXPECT_EQ(pep::encodeBase64URL("foobar"), "Zm9vYmFy");
}

// Test vectors from RFC 4648
TEST(Base64Test, TestURLDecoding) {
  EXPECT_EQ(pep::decodeBase64URL(""), "");
  EXPECT_EQ(pep::decodeBase64URL("Zg"), "f");
  EXPECT_EQ(pep::decodeBase64URL("Zm8"), "fo");
  EXPECT_EQ(pep::decodeBase64URL("Zm9v"), "foo");
  EXPECT_EQ(pep::decodeBase64URL("Zm9vYg"), "foob");
  EXPECT_EQ(pep::decodeBase64URL("Zm9vYmE"), "fooba");
  EXPECT_EQ(pep::decodeBase64URL("Zm9vYmFy"), "foobar");
}

}
