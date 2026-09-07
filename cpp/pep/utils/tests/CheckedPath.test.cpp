#include <pep/utils/CheckedPath.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

CheckedPath operator""_checkedPath(const char* path, std::size_t len) {
  return CheckedPath::FromTrusted(std::string_view(path, len));
}

TEST(CheckedPath, safetyChecks) {
  EXPECT_THROW(CheckedPath{}.path(), std::invalid_argument) << "Accessing empty path should throw";
}

// Most things are already covered by File.test.cpp

TEST(CheckedRelativePath, safetyChecks) {
  EXPECT_NO_THROW(CheckedRelativePath("abc/def"));
  EXPECT_NO_THROW(CheckedRelativePath("abc/"));
  EXPECT_NO_THROW(CheckedRelativePath("."));
  EXPECT_THROW(CheckedRelativePath(".."), std::invalid_argument);
}

TEST(CheckedRelativeFilePath, safetyChecks) {
  EXPECT_NO_THROW(CheckedRelativeFilePath("abc/def"));
  EXPECT_THROW(CheckedRelativeFilePath("abc/"), std::invalid_argument);
  EXPECT_THROW(CheckedRelativeFilePath("."), std::invalid_argument);
  EXPECT_THROW(CheckedRelativeFilePath(".."), std::invalid_argument);
  EXPECT_THROW((void) (CheckedRelativeFilePath("abc") / CheckedRelativeFilePath{}), std::invalid_argument);
}

TEST(CheckedFileName, safetyChecks) {
  EXPECT_NO_THROW(CheckedFileName("abc"));
  EXPECT_THROW(CheckedFileName("abc/def"), std::invalid_argument);
  EXPECT_THROW(CheckedFileName("."), std::invalid_argument);
}

TEST(CheckedPath, concat) {
  EXPECT_EQ("abc/def"_checkedPath + "-pending", "abc/def-pending"_checkedPath);
  EXPECT_THROW((void) ("abc/."_checkedPath + "-pending"), std::invalid_argument);
  EXPECT_THROW((void) ("abc/def"_checkedPath + "ghi/jkl"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_THROW((void) ("abc/NU"_checkedPath + "L"), std::invalid_argument);
#endif
}

TEST(CheckedFileName, concat) {
  EXPECT_EQ(CheckedFileName("abc") + "-pending", "abc-pending"_checkedPath);
  EXPECT_THROW((void) (CheckedFileName("abc") + "ghi/jkl"), std::invalid_argument);

#ifdef _WIN32
  EXPECT_THROW((void) (CheckedFileName("NU") + "L"), std::invalid_argument);
#endif
}

}
