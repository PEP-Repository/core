#include <pep/utils/MapUtils.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

namespace {

template <typename T>
auto CopyPointerToOptional(T* ptr) -> std::optional<T> { return ptr ? std::optional{*ptr} : std::nullopt; }

TEST(MapUtils, ReserveToMatch) {
  std::vector<int> container;
  {
    const auto property = "capacity increases to match source";
    pep::ReserveToMatch(container, std::vector<int>());
    EXPECT_GE(container.capacity(), 0) << property; // edge case
    pep::ReserveToMatch(container, std::vector<char>(10));
    EXPECT_GE(container.capacity(), 10) << property;
    pep::ReserveToMatch(container, std::vector<bool>(100));
    EXPECT_GE(container.capacity(), 100) << property;
  }
  {
    const auto property = "capacity never decreases";
    pep::ReserveToMatch(container, std::vector<int>(1));
    EXPECT_GE(container.capacity(), 100) << property;
    pep::ReserveToMatch(container, std::vector<int>()); // edge case
    EXPECT_GE(container.capacity(), 100) << property;
  }
}

TEST(MapUtils, MakeUnorderedPointerSet) {
  using namespace std::ranges;

  const auto original = std::vector{'A', 'B', 'C', 'C', 'C', 'B', 'D'};
  const auto pointsToOriginal = [&original](auto* ptr) {
    return any_of(original, [ptr](const auto& val) { return std::addressof(val) == ptr; });
  };
  const auto pointsToAddress = [](auto* address) { return [address](auto* ptr) { return ptr == address; }; };
  const auto pointsToValue = [](auto value) { return [value](auto* ptr) { return *ptr == value; }; };

  const auto result = pep::MakeUnorderedPointerSet(original);

  {
    const auto property = "returns (first) a pointer to each unique value";
    EXPECT_TRUE(all_of(result.first, pointsToOriginal)) << property;
    EXPECT_EQ(count_if(result.first, pointsToValue('A')), 1) << property;
    EXPECT_EQ(count_if(result.first, pointsToValue('B')), 1) << property;
    EXPECT_EQ(count_if(result.first, pointsToValue('C')), 1) << property;
    EXPECT_EQ(count_if(result.first, pointsToValue('D')), 1) << property;
  }
  {
    const auto property = "returns (second) a pointer to each duplicate value";
    EXPECT_TRUE(all_of(result.second, pointsToOriginal)) << property;
    EXPECT_EQ(count_if(result.second, pointsToValue('B')), 1) << property;
    EXPECT_EQ(count_if(result.second, pointsToValue('C')), 2) << property;
  }
  {
    const auto property = "returns exactly one pointer to each value";
    const auto isReturnedOnce = [&](auto& value) {
      return count_if(result.first, pointsToAddress(&value)) + count_if(result.second, pointsToAddress(&value)) == 1;
    };
    EXPECT_TRUE(all_of(original, isReturnedOnce)) << property;
  }
}

TEST(MapUtils, IsSubset) {
  // empty set is subset of every other set
  EXPECT_TRUE(pep::IsSubset(std::vector<int>{}, std::vector<int>{}));
  EXPECT_TRUE(pep::IsSubset(std::vector<int>{}, std::vector{1}));

  EXPECT_TRUE(pep::IsSubset(std::vector{1}, std::vector{1}));
  EXPECT_TRUE(pep::IsSubset(std::vector{1}, std::vector{1, 2}));
  EXPECT_TRUE(pep::IsSubset(std::vector{1, 2}, std::vector{1, 2, 3}));

  // unsorted
  EXPECT_TRUE(pep::IsSubset(std::vector{2, 1}, std::vector{ 2, 3, 1}));

  // not a subset
  EXPECT_FALSE(pep::IsSubset(std::vector{1}, std::vector{2}));
  EXPECT_FALSE(pep::IsSubset(std::vector{1, 2}, std::vector{2}));
}

TEST(MapUtils, TryFindDuplicateValue) {
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindDuplicateValue(std::vector<int>{})), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindDuplicateValue(std::vector<int>{1 })), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindDuplicateValue(std::vector<int>{1, 1})), 1);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindDuplicateValue(std::vector<int>{1, 2, 1})), 1);
  EXPECT_THAT(CopyPointerToOptional(pep::TryFindDuplicateValue(std::vector<int>{1, 2, 2,1})), testing::AnyOf(1, 2));

  // Make sure it supports const values
  std::vector<int> constVec{1, 1};
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindDuplicateValue(constVec)), 1);
}

TEST(MiscUtil, TryFindCommonValue) {
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{3, 2, 1}, std::vector{4, 6, 5})), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{1, 1}, std::vector{2, 2})), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{3, 4, 5}, std::vector{4, 6, 7})), 4);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{1, 1 ,3, 2}, std::vector{2, 4, 4, 5})), 2);
  EXPECT_THAT(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{2, 3, 1}, std::vector{2, 4, 3})), testing::AnyOf(2, 3));

  // edge cases: passing an empty list as argument
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector{1, 1}, std::vector<int>{})), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector<int>{}, std::vector<int>{2, 2})), std::nullopt);
  EXPECT_EQ(CopyPointerToOptional(pep::TryFindCommonValue(std::vector<int>{}, std::vector<int>{})), std::nullopt);
}

TEST(MapUtils, ContainsUniqueValues) {
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{}), true);
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1 }), true);
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2}), true);

  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 1}), false);
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 1, 2}), false);
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2, 1}), false);
  EXPECT_EQ(pep::ContainsUniqueValues(std::vector<int>{1, 2, 2,1}), false);
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
