#include <pep/utils/CollectionUtils.hpp>
#include <gtest/gtest.h>

namespace {

void TestFindLongestPrefixAtEnd(const std::string& haystack, const std::string& needle, size_t expected) {
  auto found = pep::FindLongestPrefixAtEnd(haystack, needle);
  EXPECT_EQ(found, expected) << "Found " << found << " starting character(s) of \"" << needle << "\" at the end of \"" << haystack << "\", but expected " << expected;
}

TEST(CollectionUtils, FindLongestPrefixAtEnd) {
  TestFindLongestPrefixAtEnd("", "1234", 0U);

  TestFindLongestPrefixAtEnd("1234567890", "1234", 0U);
  TestFindLongestPrefixAtEnd("1234567890", "0123", 1U);
  TestFindLongestPrefixAtEnd("1234567890", "9012", 2U);
  TestFindLongestPrefixAtEnd("1234567890", "8901", 3U);
  TestFindLongestPrefixAtEnd("1234567890", "7890", 4U);

  TestFindLongestPrefixAtEnd("1234", "1234567890", 4U);
  TestFindLongestPrefixAtEnd("1234", "2345678901", 3U);
  TestFindLongestPrefixAtEnd("1234", "3456789012", 2U);
  TestFindLongestPrefixAtEnd("1234", "4567890123", 1U);
  TestFindLongestPrefixAtEnd("1234", "5678901234", 0U);

  TestFindLongestPrefixAtEnd("1234567890", "00", 1U);
  TestFindLongestPrefixAtEnd("1234567890", "9", 0U);
  TestFindLongestPrefixAtEnd("1234567890", "9090", 2U);

  TestFindLongestPrefixAtEnd("11111111110", "1111", 0U);
  TestFindLongestPrefixAtEnd("11111110111", "1111", 3U);
  TestFindLongestPrefixAtEnd("11111111111", "1011", 1U);
}

TEST(CollectionUtils_FillVectorToCapacity, simple) {
  // Arrange
  std::vector<std::string> source{ "A", "B", "C", "D" };
  std::vector<std::string> dest{};
  size_t capacity{ 1024 };

  //Act
  size_t length = pep::FillVectorToCapacity(dest,source, capacity);

  //Assert
  std::vector<std::string> expected{ "A", "B", "C", "D" };
  ASSERT_EQ(dest, expected);
  ASSERT_EQ(length, 4);
}

TEST(CollectionUtils_FillVectorToCapacity, capacityZero) {
  // Arrange
  std::vector<std::string> source{ "A", "B", "C", "D" };
  std::vector<std::string> dest{};
  size_t capacity{ 0 };

  //Act
  size_t length = pep::FillVectorToCapacity(dest, source, capacity);

  //Assert
  std::vector<std::string> expected{ };
  ASSERT_EQ(dest, expected);
  ASSERT_EQ(length, 0);
}


TEST(CollectionUtils_FillVectorToCapacity, CapacityLimited) {
  // Arrange
  std::vector<std::string> source{ "A", "B", "C", "D" };
  std::vector<std::string> dest{};
  size_t capacity{ 2 };

  //Act
  size_t length = pep::FillVectorToCapacity(dest, source, capacity);

  //Assert
  std::vector<std::string> expected{ "A", "B"};
  ASSERT_EQ(dest, expected);
  ASSERT_EQ(length, 2);
}

TEST(CollectionUtils_FillVectorToCapacity, OffsetLimited) {
  // Arrange
  std::vector<std::string> source{ "A", "B", "C", "D" };
  std::vector<std::string> dest{};
  size_t capacity{ 1024 };
  size_t offset{ 2 };

  //Act
  size_t length = pep::FillVectorToCapacity(dest, source, capacity, offset);

  //Assert
  std::vector<std::string> expected{"C", "D" };
  ASSERT_EQ(dest, expected);
  ASSERT_EQ(length, 2);
}

}
