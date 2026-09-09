#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <gtest/gtest.h>

TEST(AccessManagerMessages, AliasNamesNormalizeToAliasedTypes) {
  static_assert(std::is_same_v<pep::ColumnAccess, pep::ColumnAccessResponse>, "One of these types aliases the other");

  EXPECT_EQ(pep::GetNormalizedTypeName<pep::ColumnAccess>(), "ColumnAccess") << "The ColumnAccess type is defined (as opposed to aliased)";
  EXPECT_EQ(pep::GetNormalizedTypeName<pep::ColumnAccessResponse>(), pep::GetNormalizedTypeName<pep::ColumnAccess>()) << "Alias and aliased type do not normalize to the same name";
}
