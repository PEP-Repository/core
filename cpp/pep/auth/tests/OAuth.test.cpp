#include <gtest/gtest.h>

#include <pep/auth/OAuthToken.hpp>

namespace {

//std::string validTokenJson = "{\"sub\":\"User1\",\"group\":\"Group1\",\"iat\":1000000000,\"exp\":2000000000}";
const auto validToken = pep::OAuthToken::Parse("eyJzdWIiOiJVc2VyMSIsImdyb3VwIjoiR3JvdXAxIiwiaWF0IjoxMDAwMDAwMDAwLCJleHAiOjIwMDAwMDAwMDB9.XTsyC65-0kqZ5G81C_w3lZ32Bx91qhztgxMc629iosg");

//std::string expiredTokenJson = "{\"sub\":\"User1\",\"group\":\"Group1\",\"iat\":1000000000,\"exp\":1300000000}";
const auto expiredToken = pep::OAuthToken::Parse("eyJzdWIiOiJVc2VyMSIsImdyb3VwIjoiR3JvdXAxIiwiaWF0IjoxMDAwMDAwMDAwLCJleHAiOjEzMDAwMDAwMDB9.enDWBMmr1K2f_GLQaQEWub_pyhsxG8nvYbbpyU9AgIs");

//std::string futureTokenJson = "{\"sub\":\"User1\",\"group\":\"Group1\",\"iat\":2000000000,\"exp\":2300000000}";
const auto futureToken = pep::OAuthToken::Parse("eyJzdWIiOiJVc2VyMSIsImdyb3VwIjoiR3JvdXAxIiwiaWF0IjoyMDAwMDAwMDAwLCJleHAiOjIzMDAwMDAwMDB9.6JLBI-EY2dG_06B9L1feQSG90W_Wg095syQtZnz035o");

// Same data as valid token, but used as HMAC key "InvalidKey"
const auto invalidToken = pep::OAuthToken::Parse("eyJzdWIiOiJVc2VyMSIsImdyb3VwIjoiR3JvdXAxIiwiaWF0IjoxMDAwMDAwMDAwLCJleHAiOjIwMDAwMDAwMDB9.Ahn96DWxMW0LjK8Mf10MzdYhN8V34dNJdDzfDOM-R_o");

const std::string oauthSecret = "SecretKey";
const std::string username = "User1";
const std::string group = "Group1";

TEST(OAuthTest, ValidToken) {
  EXPECT_TRUE(validToken.verify(oauthSecret, username, group));
}

TEST(OAuthTest, InvalidUser) {
  EXPECT_FALSE(validToken.verify(oauthSecret, "InvalidUser", group));
}

TEST(OAuthTest, EmptyUser) {
  EXPECT_FALSE(validToken.verify(oauthSecret, "", group));
}

TEST(OAuthTest, InvalidGroup) {
  EXPECT_FALSE(validToken.verify(oauthSecret, username, "InvalidGroup"));
}

TEST(OAuthTest, EmptyGroup) {
  EXPECT_FALSE(validToken.verify(oauthSecret, username, ""));
}

TEST(OAuthTest, ExpiredToken) {
  EXPECT_FALSE(expiredToken.verify(oauthSecret, username, group));
}

TEST(OAuthTest, FutureToken) {
  EXPECT_FALSE(futureToken.verify(oauthSecret, username, group));
}

TEST(OAuthTest, InvalidToken) {
  EXPECT_FALSE(invalidToken.verify(oauthSecret, username, group));
}

}
