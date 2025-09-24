#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <pep/structuredoutput/IndexedStringPool.hpp>

namespace {
using pep::structuredOutput::IndexedStringPool;
using testing::ElementsAre;

template <typename T>
std::string ToString(const T& t) noexcept {
  return std::to_string(t);
}

std::string Reverse(const std::string& str) {
  auto copy = str;
  std::ranges::reverse(copy);
  return copy;
}

TEST(structuredOutputStringPool, IsEmptyByDefault) {
  const auto arbitraryProjection = ToString<int>;
  const auto pool = IndexedStringPool<int>{arbitraryProjection};

  EXPECT_TRUE(pool.all().empty());
}

TEST(structuredOutputStringPool, PreservesOrderOfAddition) {
  const auto projection = ToString<int>;
  auto pool = IndexedStringPool<int>{projection};

  const auto first = *pool.map(3);
  const auto second = *pool.map(1);
  const auto third = *pool.map(2);

  EXPECT_THAT(pool.all(), ElementsAre(first, second, third));
}

TEST(structuredOutputStringPool, Ptr_IndexMatchesTheOrderOfAddition) {
  auto pool = IndexedStringPool<std::string>{Reverse};

  const auto zero = pool.map("zero");
  const auto one = pool.map("one");
  const auto one_again = pool.map("one");
  const auto two = pool.map("two");

  EXPECT_EQ(zero.index(), 0);
  EXPECT_EQ(one.index(), 1);
  EXPECT_EQ(one_again.index(), 1);
  EXPECT_EQ(two.index(), 2);
}

TEST(structuredOutputStringPool, Ptr_DereferencingReturnsTheMappedValue) {
  auto pool = IndexedStringPool<std::string>{Reverse};
  const auto reversed = pool.map("123");

  EXPECT_EQ(*reversed, "321");
}

TEST(structuredOutputStringPool, ValuesWithTheSameProjectionAreMappedToTheSameObject) {
  const auto stringOfSquare = [](int x) { return std::to_string(x * x); };
  auto pool = IndexedStringPool<int>{stringOfSquare};

  const auto fiveSquared = pool.map(5);
  const auto fiveSquaredAgain = pool.map(5);
  EXPECT_EQ(fiveSquared->data(), fiveSquaredAgain->data())
      << "equal input should map to the same string instance";

  const auto twoSquared = pool.map(2);
  const auto minusTwoSquared = pool.map(-2);
  EXPECT_EQ(twoSquared->data(), minusTwoSquared->data())
      << "equal projections should map to the same string instance";
}

} // namespace
