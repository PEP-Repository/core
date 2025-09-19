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

}
