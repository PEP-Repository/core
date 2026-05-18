#include <pep/versioning/SemanticVersion.hpp>
#include <gtest/gtest.h>

namespace {

TEST(Semver, Equal) {
  auto first = pep::SemanticVersion(1U, 2U, 3U, 4U);
  auto second = pep::SemanticVersion(1U, 2U, 3U, 4U);
  EXPECT_EQ(first, second) << "SemanticVersion not compared correctly";
}

TEST(Semver, NotEqual) {
  auto first = pep::SemanticVersion(1U, 2U, 3U, 4U);
  auto second = pep::SemanticVersion(1U, 2U, 3U, 5U);
  EXPECT_NE(first, second) << "SemanticVersion not compared correctly";
}

TEST(Semver, MajorVersionDiffers) {
  auto lower = pep::SemanticVersion(6U, 0U, 0U, 0U);
  auto higher = pep::SemanticVersion(10U, 0U, 0U, 0U);

  EXPECT_LT(lower, higher) << "SemanticVersion not compared correctly";
}

TEST(Semver, MinorVersionDiffers) {
  auto lower = pep::SemanticVersion(10U, 2U, 0U, 0U);
  auto higher = pep::SemanticVersion(10U, 3U, 0U, 0U);

  EXPECT_LT(lower, higher) << "SemanticVersion not compared correctly";
}

TEST(Semver, BuildDiffers) {
  auto lower = pep::SemanticVersion(10U, 3U, 123456U, 0U);
  auto higher = pep::SemanticVersion(10U, 3U, 123457U, 0U);

  EXPECT_LT(lower, higher) << "SemanticVersion not compared correctly";
}

TEST(Semver, RevisionDiffers) {
  auto lower = pep::SemanticVersion(10U, 3U, 123456U, 2345U);
  auto higher = pep::SemanticVersion(10U, 3U, 123456U, 2346U);

  EXPECT_LT(lower, higher) << "SemanticVersion not compared correctly";
}

TEST(Semver, ExternalComparisonEquivalent) {
  auto first = pep::SemanticVersion(1U, 2U, 3U, 4U);
  auto second = pep::SemanticVersion(1U, 2U, 3U, 5U);

  EXPECT_TRUE(pep::IsSemanticVersionEquivalent(first, second)) << "SemanticVersion not compared correctly";
}
TEST(Semver, ExternalComparisonNotEquivalent) {
  auto first = pep::SemanticVersion(1U, 2U, 3U, 4U);
  auto second = pep::SemanticVersion(1U, 2U, 4U, 5U);

  EXPECT_FALSE(pep::IsSemanticVersionEquivalent(first, second)) << "SemanticVersion not compared correctly";
}

}
