#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/utils/NormalizedTypeNaming.hpp> // for GetNormalizedTypeName

#include <gtest/gtest.h>

#include <type_traits>

TEST(AccessManagerMessages, AliasNamesNormalizeToAliasedTypes) {
  static_assert(std::is_same_v<pep::ColumnAccess, pep::ColumnAccessResponse>, "One of these types must alias the other");

  EXPECT_EQ(pep::GetNormalizedTypeName<pep::ColumnAccess>(), "ColumnAccess") << "The ColumnAccess type must be defined (as opposed to aliased)";
  EXPECT_EQ(pep::GetNormalizedTypeName<pep::ColumnAccessResponse>(), pep::GetNormalizedTypeName<pep::ColumnAccess>()) << "Alias and aliased type do not normalize to the same name";
}
