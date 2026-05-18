#include <gtest/gtest.h>
#include <memory>
#include <pep/utils/OptionalUtils.hpp>

namespace {

using namespace pep;

TEST(OptionalUtils, AsOptionalRef_and_AsOptionalCRef) {
  constexpr auto integer = 42;
  EXPECT_EQ(std::addressof(AsOptionalRef(&integer).value()), std::addressof(integer));
  EXPECT_EQ(std::addressof(AsOptionalCRef(&integer).value()), std::addressof(integer));

  EXPECT_FALSE(AsOptionalRef<float>(nullptr).has_value());
  EXPECT_FALSE(AsOptionalCRef<float>(nullptr).has_value());
}

TEST(OptionalUtils, AsRef_and_AsCRef) {
  auto value = 64.35;
  auto refToVal = AsOptionalRef(&value);
  auto constRefToVal = AsOptionalCRef(&value);
  auto noRef = AsOptionalRef<int>(nullptr);
  auto constNoRef = AsOptionalCRef<char>(nullptr);

  EXPECT_EQ(std::addressof(AsRef(refToVal)), std::addressof(value));
  EXPECT_EQ(std::addressof(AsCRef(refToVal)), std::addressof(value));
  EXPECT_EQ(std::addressof(AsCRef(constRefToVal)), std::addressof(value));

  EXPECT_THROW(AsRef(noRef), std::bad_optional_access);
  EXPECT_THROW(AsCRef(noRef), std::bad_optional_access);
  EXPECT_THROW(AsCRef(constNoRef), std::bad_optional_access);
}

TEST(OptionalUtils, AsPtr_and_AsPtrToConst) {
  auto value = 50.0f;
  auto refToVal = AsOptionalRef(&value);
  auto constRefToVal = AsOptionalCRef(&value);
  auto noRef = AsOptionalRef<int>(nullptr);
  auto constNoRef = AsOptionalCRef<char>(nullptr);

  EXPECT_EQ(AsPtr(refToVal), std::addressof(value));
  EXPECT_EQ(AsPtrToConst(refToVal), std::addressof(value));
  EXPECT_EQ(AsPtrToConst(constRefToVal), std::addressof(value));

  EXPECT_EQ(AsPtr(noRef), nullptr);
  EXPECT_EQ(AsPtrToConst(noRef), nullptr);
  EXPECT_EQ(AsPtrToConst(constNoRef), nullptr);
}

} // namespace
