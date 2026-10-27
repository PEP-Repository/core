#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <gtest/gtest.h>

namespace {
using namespace pep;

TEST(AccessManagerSerializers, ParticipantGroupAccessModesRoundTrip) {
  ParticipantGroupAccess input;
  input.participantGroups.emplace("groupWithoutModes", SubjectAccessModes::None);
  input.participantGroups.emplace("groupWithAccess", SubjectAccessModes::Access);
  input.participantGroups.emplace("groupWithAllModes", SubjectAccessModes::All);

  const auto output = Serialization::FromString<ParticipantGroupAccess>(Serialization::ToString(input));
  EXPECT_EQ(output.participantGroups, input.participantGroups);
}

} // namespace
