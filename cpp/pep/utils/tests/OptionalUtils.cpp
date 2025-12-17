#include <gtest/gtest.h>
#include <memory>
#include <pep/utils/OptionalUtils.hpp>

namespace {

using namespace pep;

TEST(OptionalUtils, AsOptionalRef_and_AsOptionalCRef) {
  constexpr auto integer = 42;
  EXPECT_EQ(std::addressof(AsOptionalRef(&integer).value().get()), std::addressof(integer));
  EXPECT_EQ(std::addressof(AsOptionalCRef(&integer).value().get()), std::addressof(integer));

  EXPECT_FALSE(AsOptionalRef<float>(nullptr).has_value());
  EXPECT_FALSE(AsOptionalCRef<float>(nullptr).has_value());
}

} // namespace
