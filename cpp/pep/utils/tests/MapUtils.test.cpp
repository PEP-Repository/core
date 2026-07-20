#include <pep/utils/MapUtils.hpp>
#include <gtest/gtest.h>

namespace {

TEST(MapUtils, IsSubset) {
  // empty set is subset of every other set
  ASSERT_TRUE(pep::IsSubset(std::vector<int>{}, std::vector<int>{}));
  ASSERT_TRUE(pep::IsSubset(std::vector<int>{}, std::vector{1}));

  ASSERT_TRUE(pep::IsSubset(std::vector{1}, std::vector{1}));
  ASSERT_TRUE(pep::IsSubset(std::vector{1}, std::vector{1, 2}));
  ASSERT_TRUE(pep::IsSubset(std::vector{1, 2}, std::vector{1, 2, 3}));

  // unsorted
  ASSERT_TRUE(pep::IsSubset(std::vector{2, 1}, std::vector{ 2, 3, 1}));

  // not a subset
  ASSERT_FALSE(pep::IsSubset(std::vector{1}, std::vector{2}));
  ASSERT_FALSE(pep::IsSubset(std::vector{1, 2}, std::vector{2}));
}

TEST(MapUtils, TryFindDuplicateValue) {
  ASSERT_EQ(pep::TryFindDuplicateValue(std::vector<int>{}), std::nullopt);
  ASSERT_EQ(pep::TryFindDuplicateValue(std::vector<int>{1 }), std::nullopt);
  ASSERT_EQ(pep::TryFindDuplicateValue(std::vector<int>{1, 1}), 1);
  ASSERT_EQ(pep::TryFindDuplicateValue(std::vector<int>{1, 2, 1}), 1);
  ASSERT_EQ(pep::TryFindDuplicateValue(std::vector<int>{1, 2, 2,1}), 1);
}

TEST(MapUtils, ContainsUniqueValues) {
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{}), true);
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1 }), true);
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2}), true);

  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 1}), false);
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 1, 2}), false);
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2, 1}), false);
  ASSERT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2, 2,1}), false);
}

TEST(InsertNonDuplicates, AllowsNonDuplicatesAndRejectsDuplicates) {
  using Vec = std::vector<std::string>;
  using Set = std::set<std::string>;

  std::set<std::string> dest;

  EXPECT_NO_THROW(pep::InsertNonDuplicates(dest, Vec{ "A" }));
  EXPECT_EQ(dest, (Set{ "A" })); // extra parenthesis to help compiler parse the macro

  EXPECT_NO_THROW(pep::InsertNonDuplicates(dest, Vec{ "B", "C" }));
  EXPECT_EQ(dest, (Set{ "A", "B", "C" }));

  EXPECT_NO_THROW(pep::InsertNonDuplicates(dest, std::vector<std::string>{})); // edge case
  EXPECT_EQ(dest, (Set{ "A", "B", "C" }));

  // Don't test contents of "dest" after exceptions: the function doesn't provide a strong exception guarantee
  EXPECT_ANY_THROW(pep::InsertNonDuplicates(dest, Vec{ "D", "E", "B" })) << "throws on existing duplicate in destination set";
  EXPECT_ANY_THROW(pep::InsertNonDuplicates(dest, std::vector<std::string>{"F", "G", "F"})) << "throws on duplicate in source set";
}

TEST(InsertNonDuplicates, ReturnsLastInsertedItem) {
  std::set<std::string> dest;

  EXPECT_EQ(*pep::InsertNonDuplicates(dest, std::vector<std::string>{ "A", "B", "C"}).first, "C");
  EXPECT_EQ(*pep::InsertNonDuplicates(dest, std::vector<std::string>{ "D"}).first, "D");
  EXPECT_EQ(pep::InsertNonDuplicates(dest, std::vector<std::string>{}).first, dest.end()); // edge case
}

TEST(InsertNonDuplicates, ReturnsInsertedItemCount) {
  std::set<std::string> dest;

  EXPECT_EQ(pep::InsertNonDuplicates(dest, std::vector<std::string>{ "A", "B", "C"}).second, 3);
  EXPECT_EQ(pep::InsertNonDuplicates(dest, std::vector<std::string>{ "D"}).second, 1);
  EXPECT_EQ(pep::InsertNonDuplicates(dest, std::vector<std::string>{}).second, 0); // edge case
}

}
