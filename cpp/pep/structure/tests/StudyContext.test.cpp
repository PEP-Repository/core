#include <gtest/gtest.h>

#include <pep/structure/StudyContext.hpp>

#include <stdexcept>

namespace {

pep::StudyContexts MakeContexts() {
  return pep::StudyContexts({pep::StudyContext("first"), pep::StudyContext("second")});
}

TEST(StudyContextTest, RejectsInvalidIds) {
  EXPECT_THROW(pep::StudyContext(""), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("with space"), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("with,comma"), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("with.period"), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("with-dash"), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("with\ttab"), std::runtime_error);
  EXPECT_THROW(pep::StudyContext("acc\xC3\xA9nted"), std::runtime_error); // UTF-8 encoding of e-acute
}

TEST(StudyContextTest, AcceptsAlphanumericsAndUnderscores) {
  EXPECT_EQ(pep::StudyContext("Study_42").getId(), "Study_42");
}

TEST(StudyContextTest, FirstItemBecomesDefault) {
  auto contexts = MakeContexts();
  ASSERT_EQ(contexts.getItems().size(), 2U);
  EXPECT_TRUE(contexts.getItems().front().isDefault());
  EXPECT_FALSE(contexts.getItems().back().isDefault());
  ASSERT_NE(contexts.getDefault(), nullptr);
  EXPECT_EQ(contexts.getDefault()->getId(), "first");
}

TEST(StudyContextTest, EmptyInitializationProducesSingleDefault) {
  pep::StudyContexts contexts((std::vector<pep::StudyContext>()));
  ASSERT_EQ(contexts.getItems().size(), 1U);
  EXPECT_TRUE(contexts.getItems().front().isDefault());
  EXPECT_EQ(contexts.getItems().front().getId(), "");
}

TEST(StudyContextTest, DefaultConstructedContextsHaveNoDefault) {
  pep::StudyContexts contexts;
  EXPECT_TRUE(contexts.getItems().empty());
  EXPECT_EQ(contexts.getDefault(), nullptr);
}

TEST(StudyContextTest, InitializationRejectsExplicitDefault) {
  auto contexts = MakeContexts();
  EXPECT_THROW(pep::StudyContexts duplicate(contexts.getItems()), std::runtime_error);
}

TEST(StudyContextTest, InitializationRejectsDuplicateIds) {
  EXPECT_THROW(
      pep::StudyContexts({pep::StudyContext("first"), pep::StudyContext("second"), pep::StudyContext("first")}),
      std::runtime_error);
  // Ids are compared case-insensitively
  EXPECT_THROW(
      pep::StudyContexts({pep::StudyContext("first"), pep::StudyContext("FIRST")}),
      std::runtime_error);
}

TEST(StudyContextTest, GetIdIfNonDefault) {
  auto contexts = MakeContexts();
  EXPECT_EQ(contexts.getDefault()->getIdIfNonDefault(), "");
  EXPECT_EQ(contexts.getById("second").getIdIfNonDefault(), "second");
}

TEST(StudyContextTest, EmptyContextStringMatchesOnlyDefault) {
  auto contexts = MakeContexts();
  EXPECT_TRUE(contexts.getDefault()->matches(""));
  EXPECT_FALSE(contexts.getById("second").matches(""));
}

TEST(StudyContextTest, MatchesCommaSeparatedIds) {
  auto contexts = MakeContexts();
  const auto& second = contexts.getById("second");
  EXPECT_TRUE(second.matches("second"));
  EXPECT_TRUE(second.matches("first,second"));
  EXPECT_FALSE(second.matches("first,third"));
  EXPECT_FALSE(contexts.getDefault()->matches("second"));
}

TEST(StudyContextTest, MatchesCaseInsensitively) {
  auto contexts = MakeContexts();
  const auto& second = contexts.getById("second");
  EXPECT_TRUE(second.matches("SECOND"));
  EXPECT_TRUE(second.matches("First,SeCoNd"));
}

TEST(StudyContextTest, MatchesShortPseudonymByStudyContext) {
  auto contexts = MakeContexts();
  pep::ShortPseudonymDefinition matching("ShortPseudonym.second.Test", "TEST-", 5, std::nullopt, 0, false, "", "second");
  pep::ShortPseudonymDefinition other("ShortPseudonym.Test", "TEST-", 5, std::nullopt, 0, false, "", "");
  EXPECT_TRUE(contexts.getById("second").matchesShortPseudonym(matching));
  EXPECT_FALSE(contexts.getById("second").matchesShortPseudonym(other));
  EXPECT_TRUE(contexts.getDefault()->matchesShortPseudonym(other));
}

TEST(StudyContextTest, AdministeringAssessorColumnName) {
  auto contexts = MakeContexts();
  EXPECT_EQ(contexts.getDefault()->getAdministeringAssessorColumnName(1), "Visit1.Assessor");
  EXPECT_EQ(contexts.getById("second").getAdministeringAssessorColumnName(2), "second.Visit2.Assessor");
}

TEST(StudyContextTest, EqualityComparesIdAndDefaultFlag) {
  auto contexts = MakeContexts();
  EXPECT_TRUE(pep::StudyContext("second") == contexts.getById("second"));
  EXPECT_TRUE(pep::StudyContext("SECOND") == contexts.getById("second")); // ids are compared case-insensitively
  EXPECT_TRUE(pep::StudyContext("third") != contexts.getById("second"));
  EXPECT_TRUE(pep::StudyContext("first") != *contexts.getDefault()); // same id but not flagged as default
}

TEST(StudyContextsTest, ContainsAddAndRemove) {
  auto contexts = MakeContexts();
  pep::StudyContext third("third");
  EXPECT_FALSE(contexts.contains(third));

  contexts.add(third);
  EXPECT_TRUE(contexts.contains(third));
  EXPECT_THROW(contexts.add(third), std::runtime_error);

  contexts.remove(third);
  EXPECT_FALSE(contexts.contains(third));
  EXPECT_THROW(contexts.remove(third), std::runtime_error);
}

TEST(StudyContextsTest, AddRejectsDuplicateIdCaseInsensitively) {
  auto contexts = MakeContexts();
  EXPECT_THROW(contexts.add(pep::StudyContext("SECOND")), std::runtime_error);
  // Also rejects the id of the default context, even though the added context is not flagged as default
  EXPECT_THROW(contexts.add(pep::StudyContext("first")), std::runtime_error);
}

TEST(StudyContextsTest, AddRejectsSecondDefault) {
  auto contexts = MakeContexts();
  pep::StudyContexts others({pep::StudyContext("third")});
  EXPECT_THROW(contexts.add(*others.getDefault()), std::runtime_error);
}

TEST(StudyContextsTest, GetById) {
  auto contexts = MakeContexts();
  EXPECT_EQ(contexts.getById("second").getId(), "second");
  EXPECT_EQ(contexts.getById("SECOND").getId(), "second"); // case-insensitive lookup returns the configured casing
  EXPECT_THROW(contexts.getById("nonexistent"), std::runtime_error);
}

TEST(StudyContextsTest, ToStringJoinsIdsWithCommas) {
  EXPECT_EQ(MakeContexts().toString(), "first,second");
  EXPECT_EQ(pep::StudyContexts().toString(), "");
}

TEST(StudyContextsTest, ParseEmptyStringYieldsDefault) {
  auto contexts = MakeContexts();
  auto parsed = contexts.parse("");
  ASSERT_EQ(parsed.getItems().size(), 1U);
  EXPECT_EQ(parsed.getItems().front().getId(), "first");
}

TEST(StudyContextsTest, ParseLooksUpIds) {
  auto contexts = MakeContexts();
  auto parsed = contexts.parse("second,first");
  ASSERT_EQ(parsed.getItems().size(), 2U);
  EXPECT_EQ(parsed.getItems().front().getId(), "second");
  EXPECT_EQ(parsed.getItems().back().getId(), "first");

  EXPECT_THROW(contexts.parse("nonexistent"), std::runtime_error);
}

TEST(StudyContextsTest, ParseEmptyStringWithoutDefaultThrows) {
  pep::StudyContexts contexts;
  EXPECT_THROW(contexts.parse(""), std::runtime_error);
}

}
