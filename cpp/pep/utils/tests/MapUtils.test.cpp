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

}
