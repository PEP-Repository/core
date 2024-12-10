#include <gtest/gtest.h>

#include <pep/utils/UriEncode.hpp>

// Test UriEncoding
TEST(UriEncodeTest, TestUriEncoding) {
    EXPECT_EQ(pep::UriEncode(""), "");
    EXPECT_EQ(pep::UriEncode("A"), "A");
    EXPECT_EQ(pep::UriEncode(" "), "%20");
    EXPECT_EQ(pep::UriEncode("urn:isbn:0451450523"), "urn%3Aisbn%3A0451450523");
    EXPECT_EQ(pep::UriEncode("  ;aaa__%"), "%20%20%3Baaa__%25");
    EXPECT_EQ(pep::UriEncode("?a=\"b\""), "%3Fa%3D%22b%22");
}

// Test UriDecoding
TEST(UriEncodeTest, TestUriDecoding) {
    EXPECT_EQ(pep::UriDecode(""), "");
    EXPECT_EQ(pep::UriDecode("A"), "A");
    EXPECT_EQ(pep::UriDecode("%20"), " ");
    EXPECT_EQ(pep::UriDecode("urn%3Aisbn%3A0451450523"), "urn:isbn:0451450523");
    EXPECT_EQ(pep::UriDecode("%20%20%3Baaa__%25"), "  ;aaa__%");
    EXPECT_EQ(pep::UriDecode("%3Fa%3D%22b%22"), "?a=\"b\"");

    EXPECT_THROW(pep::UriDecode("%"), std::runtime_error);
    EXPECT_THROW(pep::UriDecode("%a"), std::runtime_error);
}
