#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/serialization/TimestampSerializer.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>

#include <string_view>
#include <vector>

using namespace pep;

namespace {

using testing::Pointwise;

PolymorphicPseudonym MakePolymorphicPseudonym(std::string_view identifier) {
  return PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), identifier);
}

LocalPseudonyms MakeLocalPseudonyms(std::string_view identifier) {
  const auto publicKey = ElgamalPublicKey::Random();
  const auto localPseudonym = LocalPseudonym::Random();
  return LocalPseudonyms{
      .accessManager = localPseudonym.encrypt(publicKey),
      .storageFacility = localPseudonym.encrypt(publicKey),
      .polymorphic = PolymorphicPseudonym::FromIdentifier(publicKey, identifier),
      .accessGroup = localPseudonym.encrypt(publicKey)
  };
}

// Matcher for an (actual, expected) pair of LocalPseudonyms, to be used with Pointwise for vector comparisons
MATCHER(LocalPseudonymsMatch, "LocalPseudonyms that are equal in all fields") {
  const auto& [actual, expected] = arg;
  return actual.accessManager == expected.accessManager && actual.storageFacility == expected.storageFacility &&
         actual.polymorphic == expected.polymorphic && actual.accessGroup == expected.accessGroup;
}

TEST(TicketingSerializers, TicketRequest2RoundTrip) {
  TicketRequest2 input; // does not support designated initialization (because of inheritance)
  input.modes = {"read", "write"};
  input.participantGroups = {"participantGroup"};
  input.accessSubjects = {MakePolymorphicPseudonym("subjectA"), MakePolymorphicPseudonym("subjectB")};
  input.referenceSubjects = {MakePolymorphicPseudonym("subjectC"), MakePolymorphicPseudonym("subjectC")};
  input.columnGroups = {"columnGroup"};
  input.columns = {"column"};
  input.requestIndexedTicket = true;
  input.includeUserGroupPseudonyms = true;

  const auto output = Serialization::FromString<TicketRequest2>(Serialization::ToString(input));
  EXPECT_EQ(output.modes, input.modes);
  EXPECT_EQ(output.participantGroups, input.participantGroups);
  EXPECT_EQ(output.accessSubjects, input.accessSubjects);
  EXPECT_EQ(output.referenceSubjects, input.referenceSubjects);
  EXPECT_EQ(output.columnGroups, input.columnGroups);
  EXPECT_EQ(output.columns, input.columns);
  EXPECT_EQ(output.requestIndexedTicket, input.requestIndexedTicket);
  EXPECT_EQ(output.includeUserGroupPseudonyms, input.includeUserGroupPseudonyms);
}

TEST(TicketingSerializers, Ticket2RoundTrip) {
  Ticket2 ticket{
      .modes = {"read"},
      .accessSubjects = {MakeLocalPseudonyms("subjectA"), MakeLocalPseudonyms("subjectB")},
      .referenceSubjects = {MakeLocalPseudonyms("subjectD")},
      .columns = {"column"},
      .userGroup = "userGroup"
  };

  const auto deserialized = Serialization::FromString<Ticket2>(Serialization::ToString(ticket));
  EXPECT_EQ(deserialized.timestamp, ticket.timestamp);
  EXPECT_EQ(deserialized.modes, ticket.modes);
  EXPECT_THAT(deserialized.accessSubjects, Pointwise(LocalPseudonymsMatch(), ticket.accessSubjects));
  EXPECT_THAT(deserialized.referenceSubjects, Pointwise(LocalPseudonymsMatch(), ticket.referenceSubjects));
  EXPECT_EQ(deserialized.columns, ticket.columns);
  EXPECT_EQ(deserialized.userGroup, ticket.userGroup);
}

} // namespace
