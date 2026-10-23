#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/serialization/Error.hpp>
#include <pep/ticketing/ColumnAccessModes.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace {
using namespace pep;

using testing::ElementsAre;

TEST(TicketMode, ToIndividualStringsListsEverySetMode) {
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::None), ElementsAre());
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::Read), ElementsAre("read"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::ReadMeta), ElementsAre("read-meta"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::Write), ElementsAre("write"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::WriteMeta), ElementsAre("write-meta"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::_bogus_AmaTranscryptionOnly), ElementsAre("enumerate"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::Read | ColumnAccessModes::WriteMeta),
      ElementsAre("read", "write-meta"));
  EXPECT_THAT(ToIndividualStrings(ColumnAccessModes::All),
      ElementsAre("read", "read-meta", "write", "write-meta", "enumerate"))
      << "modes should appear in stable order";
}

TEST(TicketMode, ToSingleStringNamesNone) {
  EXPECT_EQ(ToSingleString(ColumnAccessModes::None), "none");
}

TEST(TicketMode, ToSingleStringJoinsTheIndividualModes) {
  EXPECT_EQ(ToSingleString(ColumnAccessModes::Read), "read");
  EXPECT_EQ(ToSingleString(ColumnAccessModes::Read | ColumnAccessModes::WriteMeta), "read|write-meta");
}

TEST(TicketMode, ToSingleStringUsesTheSeparatorFromTheCaller) {
  EXPECT_EQ(ToSingleString(ColumnAccessModes::Read | ColumnAccessModes::Write, ", "), "read, write");
}

TEST(TicketMode, ParseModeAcceptsEveryMode) {
  EXPECT_EQ(ParseMode("read"), ColumnAccessModes::Read);
  EXPECT_EQ(ParseMode("read-meta"), ColumnAccessModes::ReadMeta);
  EXPECT_EQ(ParseMode("write"), ColumnAccessModes::Write);
  EXPECT_EQ(ParseMode("write-meta"), ColumnAccessModes::WriteMeta);
  EXPECT_EQ(ParseMode("enumerate"), ColumnAccessModes::_bogus_AmaTranscryptionOnly);
}

TEST(TicketMode, ParseModeRejectsUnknownModes) {
  EXPECT_THROW(ParseMode("access"), Error);
  EXPECT_THROW(ParseMode("Read"), Error) << "mode names are case sensitive";
  EXPECT_THROW(ParseMode(""), Error);
}

TEST(TicketMode, IndividualNameParsingRoundtrip) {
  for (const auto name : {"read", "read-meta", "write", "write-meta", "enumerate"}) {
    EXPECT_EQ(ToSingleString(ParseMode(name)), name);
  }
}

TEST(TicketMode, ParseModesCombinesTheNames) {
  EXPECT_EQ(ParseModes(std::vector<std::string>{}), ColumnAccessModes::None);
  EXPECT_EQ(ParseModes(std::vector<std::string>{"read"}), ColumnAccessModes::Read);
  EXPECT_EQ(ParseModes(std::vector<std::string>{"read-meta", "write-meta"}),
      ColumnAccessModes::ReadMeta | ColumnAccessModes::WriteMeta);
}

TEST(TicketMode, EffectiveModesIncludesTheImplicitlyGrantedModes) {
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::Read), ColumnAccessModes::Read | ColumnAccessModes::ReadMeta)
      << "read implies read-meta";
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::Write), ColumnAccessModes::Write)
      << "write does not imply write-meta";
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::ReadMeta), ColumnAccessModes::ReadMeta)
      << "read-meta does not imply read";
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::WriteMeta), ColumnAccessModes::WriteMeta | ColumnAccessModes::Write)
      << "write-meta implies write";
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::_bogus_AmaTranscryptionOnly), ColumnAccessModes::_bogus_AmaTranscryptionOnly);

  // combinations
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::Read | ColumnAccessModes::WriteMeta),
      ColumnAccessModes::Read | ColumnAccessModes::Write | ColumnAccessModes::ReadMeta | ColumnAccessModes::WriteMeta);

  // edge-cases
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::None), ColumnAccessModes::None);
  EXPECT_EQ(EffectiveModes(ColumnAccessModes::All), ColumnAccessModes::All);
}

TEST(TicketMode, ModesIncludeTestsImplicitlyGrantedModes) {
  EXPECT_TRUE(ImpliesModes(ColumnAccessModes::Read, ColumnAccessModes::ReadMeta));
  EXPECT_FALSE(ImpliesModes(ColumnAccessModes::Write, ColumnAccessModes::WriteMeta));
  EXPECT_FALSE(ImpliesModes(ColumnAccessModes::ReadMeta, ColumnAccessModes::Read));
  EXPECT_TRUE(ImpliesModes(ColumnAccessModes::WriteMeta, ColumnAccessModes::Write));

  // combinations
  EXPECT_TRUE(ImpliesModes(ColumnAccessModes::Read | ColumnAccessModes::WriteMeta, ColumnAccessModes::Read | ColumnAccessModes::Write));
  EXPECT_FALSE(ImpliesModes(ColumnAccessModes::Read, ColumnAccessModes::Read | ColumnAccessModes::Write));

  // edge-cases
  EXPECT_TRUE(ImpliesModes(ColumnAccessModes::None, ColumnAccessModes::None));
  EXPECT_TRUE(ImpliesModes(ColumnAccessModes::All, ColumnAccessModes::All));
}

} // namespace
