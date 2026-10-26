#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/ticketing/SubjectAccessModes.hpp>
#include <pep/serialization/Error.hpp>

namespace {
using namespace pep;

using testing::ElementsAre;

TEST(SubjectAccessModes, ToIndividualStringsListsEverySetMode) {
  EXPECT_THAT(ToIndividualStrings(SubjectAccessModes::None), ElementsAre());
  EXPECT_THAT(ToIndividualStrings(SubjectAccessModes::Access), ElementsAre("access"));
  EXPECT_THAT(ToIndividualStrings(SubjectAccessModes::Enumerate), ElementsAre("enumerate"));
  EXPECT_THAT(ToIndividualStrings(SubjectAccessModes::All),
      ElementsAre("access", "enumerate"))
      << "modes should appear in stable order";
}

TEST(SubjectAccessModes, ToSingleStringNamesNone) {
  EXPECT_EQ(ToSingleString(SubjectAccessModes::None), "none");
}

TEST(SubjectAccessModes, ToSingleStringJoinsTheIndividualModes) {
  EXPECT_EQ(ToSingleString(SubjectAccessModes::Access), "access");
  EXPECT_EQ(ToSingleString(SubjectAccessModes::All), "access|enumerate");
}

TEST(SubjectAccessModes, ToSingleStringUsesTheSeparatorFromTheCaller) {
  EXPECT_EQ(ToSingleString(SubjectAccessModes::All, ", "), "access, enumerate");
}

TEST(SubjectAccessModes, ParseSubjectAccessModeAcceptsEveryMode) {
  EXPECT_EQ(ParseSubjectAccessMode("access"), SubjectAccessModes::Access);
  EXPECT_EQ(ParseSubjectAccessMode("enumerate"), SubjectAccessModes::Enumerate);
}

TEST(SubjectAccessModes, ParseSubjectAccessModeRejectsUnknownModes) {
  EXPECT_THROW(ParseSubjectAccessMode("read"), Error) << "column access modes are not subject access modes";
  EXPECT_THROW(ParseSubjectAccessMode("Access"), Error) << "mode names are case sensitive";
  EXPECT_THROW(ParseSubjectAccessMode(""), Error);
}

TEST(SubjectAccessModes, IndividualNameParsingRoundtrip) {
  for (const auto name : {"access", "enumerate"}) {
    EXPECT_EQ(ToSingleString(ParseSubjectAccessMode(name)), name);
  }
}

TEST(SubjectAccessModes, ParseSubjectAccessModesCombinesTheNames) {
  EXPECT_EQ(ParseSubjectAccessModes(std::vector<std::string>{}), SubjectAccessModes::None);
  EXPECT_EQ(ParseSubjectAccessModes(std::vector<std::string>{"access"}), SubjectAccessModes::Access);
  EXPECT_EQ(ParseSubjectAccessModes(std::vector<std::string>{"enumerate", "access"}),
      SubjectAccessModes::Access | SubjectAccessModes::Enumerate);
}

TEST(SubjectAccessModes, HasFlagsRequiresEveryFlag) {
  EXPECT_TRUE(HasFlags(SubjectAccessModes::Access | SubjectAccessModes::Enumerate, SubjectAccessModes::Access));
  EXPECT_FALSE(HasFlags(SubjectAccessModes::Access, SubjectAccessModes::Access | SubjectAccessModes::Enumerate));
  EXPECT_TRUE(HasFlags(SubjectAccessModes::All, SubjectAccessModes::All));
  EXPECT_TRUE(HasFlags(SubjectAccessModes::None, SubjectAccessModes::None));
}

} // namespace
