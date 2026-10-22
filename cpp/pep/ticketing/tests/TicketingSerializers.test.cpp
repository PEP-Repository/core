#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/serialization/TimestampSerializer.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/ticketing/tests/LocalPseudonyms.test.hpp>

#include <vector>

using namespace pep;
using namespace pep::tests;

namespace {

using testing::Pointwise;

// Matcher for an (actual, expected) pair of LocalPseudonyms, to be used with Pointwise for vector comparisons
MATCHER(LocalPseudonymsMatch, "LocalPseudonyms that are equal in all fields") {
  const auto& [actual, expected] = arg;
  return actual.accessManager == expected.accessManager && actual.storageFacility == expected.storageFacility &&
         actual.polymorphic == expected.polymorphic && actual.accessGroup == expected.accessGroup;
}

TEST(TicketingSerializers, TicketRequest2RoundTrip) {
  const TicketRequest2 input = [](TicketRequest2 ticket = {}) { // does not support designated initialization
    const auto [subjectA, subjectB, subjectC, subjectD] = Generate<4>(SingleRandomPolymorphicPseudonym);
    ticket.modes = {"read", "write"};
    ticket.accessSubjects = {subjectA, subjectB};
    ticket.participantGroups = {"participantGroup"};
    ticket.referenceSubjects = {subjectC, subjectD};
    ticket.columnGroups = {"columnGroup"};
    ticket.columns = {"column"};
    ticket.requestIndexedTicket = true;
    ticket.includeUserGroupPseudonyms = true;
    return ticket;
  }();

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
  const auto [subjectA, subjectB, subjectC] = Generate<3>(SingleRandomLocalPseudonym);
  const auto ticket = Ticket2{
      .modes = {"read"},
      .accessSubjects = {subjectA, subjectB},
      .referenceSubjects = {subjectC},
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
