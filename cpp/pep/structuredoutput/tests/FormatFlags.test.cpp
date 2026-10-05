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
  EXPECT_THAT(ToIndividualStrings(FormatFlags::All), ElementsAre("csv", "json", "yaml")) << "flags should appear in stable order";
}

TEST(structuredOutputFormatFlags, ToSingleStringNamesNone) {
  EXPECT_EQ(ToSingleString(FormatFlags::None), "none");
}

TEST(structuredOutputFormatFlags, ToSingleStringJoinsTheIndividualFlags) {
  EXPECT_EQ(ToSingleString(FormatFlags::Csv), "csv");
  EXPECT_EQ(ToSingleString(FormatFlags::Csv | FormatFlags::Yaml), "csv|yaml");
  EXPECT_EQ(ToSingleString(FormatFlags::All), "csv|json|yaml") << "All is spelled out, not abbreviated";
}

TEST(structuredOutputFormatFlags, ToSingleStringUsesTheSeparatorFromTheCaller) {
  EXPECT_EQ(ToSingleString(FormatFlags::All, ", "), "csv, json, yaml");
}

} // namespace
