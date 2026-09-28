#include <pep/utils/StringStream.hpp>

#include <gtest/gtest.h>

#include <sstream>

namespace {

TEST(StringStream, GetUnparsed) {
  std::istringstream partial("123 remainder");
  int value{};
  partial >> value;
  EXPECT_EQ(pep::GetUnparsed(partial), " remainder");
  EXPECT_EQ(pep::GetUnparsed(std::move(partial)), " remainder");

  // Reading up to the end sets eofbit, which makes tellg fail on some implementations
  std::istringstream consumed("123");
  consumed.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  consumed >> value;
  ASSERT_TRUE(consumed.eof());
  EXPECT_EQ(pep::GetUnparsed(consumed), "");
  EXPECT_EQ(pep::GetUnparsed(std::move(consumed)), "");
}

}
