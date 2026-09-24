#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/structuredoutput/FormatFlags.hpp>

namespace {
using namespace pep::enumUtils;

using pep::structuredOutput::FormatFlags;
using pep::structuredOutput::ToIndividualStrings;
using pep::structuredOutput::ToSingleString;
using testing::ElementsAre;

TEST(structuredOutputFormatFlags, ToIndividualStringsListsEverySetFlag) {
  EXPECT_THAT(ToIndividualStrings(FormatFlags::None), ElementsAre());
  EXPECT_THAT(ToIndividualStrings(FormatFlags::Csv), ElementsAre("csv"));
  EXPECT_THAT(ToIndividualStrings(FormatFlags::Json), ElementsAre("json"));
  EXPECT_THAT(ToIndividualStrings(FormatFlags::Yaml), ElementsAre("yaml"));
  EXPECT_THAT(ToIndividualStrings(FormatFlags::Csv | FormatFlags::Yaml), ElementsAre("csv", "yaml"));
  EXPECT_THAT(ToIndividualStrings(FormatFlags::All), ElementsAre("csv", "json", "yaml"))
      << "flags should appear in declaration order";
}

TEST(structuredOutputFormatFlags, ToSingleStringNamesTheEdgeCases) {
  EXPECT_EQ(ToSingleString(FormatFlags::None), "none");
  EXPECT_EQ(ToSingleString(FormatFlags::All), "all");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Json | FormatFlags::Yaml), "all")
      << "the combination of all individual flags is equal to FormatFlags::All";
}

TEST(structuredOutputFormatFlags, ToSingleStringJoinsTheIndividualFlags) {
  EXPECT_EQ(ToSingleString(FormatFlags::Csv), "csv");
  EXPECT_EQ(ToSingleString(FormatFlags::Json), "json");
  EXPECT_EQ(ToSingleString(FormatFlags::Yaml), "yaml");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Json), "csv|json");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Yaml), "csv|yaml");
}

TEST(structuredOutputFormatFlags, ToSingleStringUsesTheSeparatorFromTheCaller) {
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Yaml, ", "), "csv, yaml");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Yaml, ""), "csvyaml");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv, ", "), "csv") << "a single flag needs no separator";
  EXPECT_EQ(ToSingleString(FormatFlags::None, ", "), "none") << "the separator does not apply to the edge cases";
  EXPECT_EQ(ToSingleString(FormatFlags::All, ", "), "all") << "the separator does not apply to the edge cases";
}

} // namespace
