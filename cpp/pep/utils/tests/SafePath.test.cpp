#include <pep/utils/SafePath.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

SafePath operator""_safePath(const char* path, std::size_t len) {
  return SafePath::FromTrusted(std::string_view(path, len));
}

TEST(SafePath, safetyChecks) {
  EXPECT_THROW(SafePath{}.path(), std::invalid_argument) << "Accessing empty path should throw";
}

// Most things are already covered by File.test.cpp

TEST(SafeRelativePath, safetyChecks) {
  EXPECT_NO_THROW(SafeRelativePath("abc/def"));
  EXPECT_NO_THROW(SafeRelativePath("abc/"));
  EXPECT_NO_THROW(SafeRelativePath("."));
  EXPECT_THROW(SafeRelativePath(".."), std::invalid_argument);
}

TEST(SafeRelativeFilePath, safetyChecks) {
  EXPECT_NO_THROW(SafeRelativeFilePath("abc/def"));
  EXPECT_THROW(SafeRelativeFilePath("abc/"), std::invalid_argument);
  EXPECT_THROW(SafeRelativeFilePath("."), std::invalid_argument);
  EXPECT_THROW(SafeRelativeFilePath(".."), std::invalid_argument);
  EXPECT_THROW((void) (SafeRelativeFilePath("abc") / SafeRelativeFilePath{}), std::invalid_argument);
}

TEST(SafeFileName, safetyChecks) {
  EXPECT_NO_THROW(SafeFileName("abc"));
  EXPECT_THROW(SafeFileName("abc/def"), std::invalid_argument);
  EXPECT_THROW(SafeFileName("."), std::invalid_argument);
}

TEST(SafePath, concat) {
  EXPECT_EQ("abc/def"_safePath + "-pending", "abc/def-pending"_safePath);
  EXPECT_THROW((void) ("abc/."_safePath + "-pending"), std::invalid_argument);
  EXPECT_THROW((void) ("abc/def"_safePath + "ghi/jkl"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_THROW((void) ("abc/NU"_safePath + "L"), std::invalid_argument);
#endif
}

TEST(SafeFileName, concat) {
  EXPECT_EQ(SafeFileName("abc") + "-pending", "abc-pending"_safePath);
  EXPECT_THROW((void) (SafeFileName("abc") + "ghi/jkl"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_THROW((void) (SafeFileName("NU") + "L"), std::invalid_argument);
#endif
}

}
