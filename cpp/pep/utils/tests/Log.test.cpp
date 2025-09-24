#include <gtest/gtest.h>
#include <pep/utils/Log.hpp>

namespace {

TEST(Logging, Escape) {
  ASSERT_EQ(pep::Logging::Escape(""), "\"\"");
  ASSERT_EQ(pep::Logging::Escape("\\"), "\"\\\\\"");
  ASSERT_EQ(pep::Logging::Escape("\""), "\"\"\"");
  ASSERT_EQ(pep::Logging::Escape("\xFF"), "\"\\x0ffffffff\"");
  ASSERT_EQ(pep::Logging::Escape("abcdefghijkmlnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{};':|?><,."), "\"abcdefghijkmlnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{};':|?><,.\"");
  ASSERT_EQ(pep::Logging::Escape("abcdefghijkmlnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{};':|?><,.\\\"\xFF"), "\"abcdefghijkmlnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{};':|?><,.\\\\\"\\x0ffffffff\"");
}

}
