#include <gtest/gtest.h>

#include <pep/structure/ColumnName.hpp>
#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <algorithm>
#include <stdexcept>

namespace {

TEST(ColumnNameTest, ConstructorStoresValueVerbatim) {
  pep::ColumnNameSection section("Raw value!");
  EXPECT_EQ(section.getValue(), "Raw value!");
}

TEST(ColumnNameTest, FromRawStringReplacesWhitespaceWithUnderscores) {
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("Foo Bar").getValue(), "Foo_Bar");
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("a\tb\nc\rd").getValue(), "a_b_c_d");
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("  leading and trailing  ").getValue(), "__leading_and_trailing__");
}

TEST(ColumnNameTest, FromRawStringStripsSpecialCharacters) {
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("Foo-Bar").getValue(), "FooBar");
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("weird!@#$%^&*()chars").getValue(), "weirdchars");
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("dots.and.détails").getValue(), "dotsanddtails");
  // A special character between spaces leaves a double underscore
  EXPECT_EQ(
      pep::ColumnNameSection::FromRawString("Baseline & Followup Measurements Extended Study").getValue(),
      "Baseline__Followup_Measurements_Extended_Study");
}

TEST(ColumnNameTest, FromRawStringKeepsAlphanumericsAndUnderscores) {
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("Already_Valid_123").getValue(), "Already_Valid_123");
  EXPECT_EQ(pep::ColumnNameSection::FromRawString("").getValue(), "");
}

TEST(ColumnNameMappingsTest, RejectsDuplicateOriginals) {
  std::vector<pep::ColumnNameMapping> entries{
    {pep::ColumnNameSection("Daily_survey_4_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail4")},
    {pep::ColumnNameSection("Daily_survey_4_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail7")},
  };
  EXPECT_THROW(pep::ColumnNameMappings mappings(entries), std::runtime_error);
}

TEST(ColumnNameMappingsTest, ShortensMangledOriginalToReadableName) {
  pep::ColumnNameMappings mappings({
    {pep::ColumnNameSection("Daily_survey_4_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail4")},
    {pep::ColumnNameSection("Daily_survey_7_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail7")},
    {pep::ColumnNameSection("Daily_survey_day_2_through_day_7"), pep::ColumnNameSection("SurvDaily")},
  });
  EXPECT_EQ(mappings.getColumnNameSectionFor("Daily_survey_4_consecutive_days_in_one_week"), "SurvDail4");
  EXPECT_EQ(mappings.getColumnNameSectionFor("Daily_survey_7_consecutive_days_in_one_week"), "SurvDail7");
  EXPECT_EQ(mappings.getColumnNameSectionFor("Daily_survey_day_2_through_day_7"), "SurvDaily");
}

TEST(ColumnNameMappingsTest, ManglesRawCastorNameBeforeMatching) {
  pep::ColumnNameMappings mappings({
    {pep::ColumnNameSection::FromRawString("Daily survey (4 opeenvolgende dagen in één week)"), pep::ColumnNameSection("SurvDail4")},
    {pep::ColumnNameSection("Baseline__Followup_Measurements_Extended_Study"), pep::ColumnNameSection("BaseFU")},
  });
  EXPECT_EQ(mappings.getColumnNameSectionFor("Daily survey (4 opeenvolgende dagen in één week)"), "SurvDail4");
  // The " & " in the raw name mangles to the double underscore in the configured original
  EXPECT_EQ(mappings.getColumnNameSectionFor("Baseline & Followup Measurements Extended Study"), "BaseFU");
}

TEST(ColumnNameMappingsTest, ReturnsMangledOriginalWhenUnmapped) {
  pep::ColumnNameMappings mappings({
    {pep::ColumnNameSection("Daily_survey_4_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail4")},
  });
  EXPECT_EQ(mappings.getColumnNameSectionFor("Daily survey (day 8)"), "Daily_survey_day_8");
}

TEST(ColumnNameMappingsTest, GetEntriesReturnsAllMappings) {
  pep::ColumnNameMappings mappings({
    {pep::ColumnNameSection("Daily_survey_4_consecutive_days_in_one_week"), pep::ColumnNameSection("SurvDail4")},
    {pep::ColumnNameSection("Daily_survey_day_2_through_day_7"), pep::ColumnNameSection("SurvDaily")},
  });

  auto entries = mappings.getEntries();
  ASSERT_EQ(entries.size(), 2U);
  auto findOriginal = [&entries](const std::string& original) {
    return std::find_if(entries.cbegin(), entries.cend(), [&original](const pep::ColumnNameMapping& entry) {
      return entry.original.getValue() == original;
    });
  };
  auto first = findOriginal("Daily_survey_4_consecutive_days_in_one_week");
  ASSERT_NE(first, entries.cend());
  EXPECT_EQ(first->mapped.getValue(), "SurvDail4");
  auto second = findOriginal("Daily_survey_day_2_through_day_7");
  ASSERT_NE(second, entries.cend());
  EXPECT_EQ(second->mapped.getValue(), "SurvDaily");
}

TEST(ColumnNameSerializersTest, ColumnNameSectionRoundTrip) {
  pep::ColumnNameSection section("Daily_survey_day_2_through_day_7");
  auto deserialized = pep::Serialization::FromString<pep::ColumnNameSection>(pep::Serialization::ToString(section));
  EXPECT_EQ(deserialized.getValue(), "Daily_survey_day_2_through_day_7");
}

TEST(ColumnNameSerializersTest, ColumnNameMappingRoundTrip) {
  pep::ColumnNameMapping mapping{pep::ColumnNameSection("Daily_survey_day_2_through_day_7"), pep::ColumnNameSection("SurvDaily")};
  auto deserialized = pep::Serialization::FromString<pep::ColumnNameMapping>(pep::Serialization::ToString(mapping));
  EXPECT_EQ(deserialized.original.getValue(), "Daily_survey_day_2_through_day_7");
  EXPECT_EQ(deserialized.mapped.getValue(), "SurvDaily");
}

}
