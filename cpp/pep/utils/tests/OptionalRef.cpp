#include <gtest/gtest.h>
#include <memory>
#include <pep/utils/OptionalRef.hpp>

namespace {

using namespace pep;

TEST(OptionalRef, FromPtr) {  // covers both Ref and CRef variants of the function
  constexpr auto integer = 42;
  EXPECT_EQ(std::addressof(OptionalRefFromPtr(&integer).value().get()), std::addressof(integer));
  EXPECT_EQ(std::addressof(OptionalCRefFromPtr(&integer).value().get()), std::addressof(integer));

  EXPECT_FALSE(OptionalRefFromPtr<float>(nullptr).has_value());
  EXPECT_FALSE(OptionalCRefFromPtr<float>(nullptr).has_value());
}

} // namespace
