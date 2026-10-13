#include <algorithm>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

#include <string_view>
#include <unistd.h>

using namespace pep;

namespace {

using testing::UnorderedElementsAre;
using std::ranges::is_sorted;

template <std::size_t N> std::array<PolymorphicPseudonym, N> RandomPolymorphicPseudonyms() {
  std::array<PolymorphicPseudonym, N> array;
  for (auto& pp : array) {
    pp = PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "arbitrary identifier");
  }
  return array;
}

TEST(ResolveDuplicatePseudonyms, RemovesDuplicatesWithinAndAcrossLists) {
  const auto subject = RandomPolymorphicPseudonyms<4>();

  ClientSideTicketRequest2 request;
  request.accessSubjects = {subject[0], subject[1], subject[0]}; // 0 twice
  request.referenceSubjects = {subject[2], subject[2], subject[1], subject[3]}; // 2 twice and 1 is also accessed

  ResolveDuplicatePseudonyms(request);

  EXPECT_THAT(request.accessSubjects, UnorderedElementsAre(subject[0], subject[1])); // keeping 1 as access subject
  EXPECT_THAT(request.referenceSubjects, UnorderedElementsAre(subject[2], subject[3]));
}

TEST(ResolveDuplicatePseudonyms, LeavesUniquePseudonymsIntact) {
  const auto subject = RandomPolymorphicPseudonyms<2>();

  ClientSideTicketRequest2 request;
  { // both list populated
    request.accessSubjects = {subject[0]};
    request.referenceSubjects = {subject[1]};

    ResolveDuplicatePseudonyms(request);

    EXPECT_THAT(request.accessSubjects, UnorderedElementsAre(subject[0]));
    EXPECT_THAT(request.referenceSubjects, UnorderedElementsAre(subject[1]));
  }
  { // edge case: no reference subjects
    request.accessSubjects = {subject[0], subject[1]};
    request.referenceSubjects = {};

    ResolveDuplicatePseudonyms(request);

    EXPECT_THAT(request.accessSubjects, UnorderedElementsAre(subject[0], subject[1]));
    EXPECT_TRUE(request.referenceSubjects.empty());
  }
  { // edge case: no access subjects
    request.accessSubjects = {};
    request.referenceSubjects = {subject[0], subject[1]};

    ResolveDuplicatePseudonyms(request);

    EXPECT_TRUE(request.accessSubjects.empty());
    EXPECT_THAT(request.referenceSubjects, UnorderedElementsAre(subject[0], subject[1]));
  }
}


TEST(ResolveDuplicatePseudonyms, SortsSubjectsAsSideEffect) {
  // Not a design goal of the function, but it is a documented side-effect, that callers could be relying on
  const auto subject = RandomPolymorphicPseudonyms<8>();

  ClientSideTicketRequest2 request;
  request.accessSubjects = {subject[0], subject[1], subject[2], subject[3]};
  request.referenceSubjects = {subject[4], subject[5], subject[6], subject[7]};

  const auto unsort = [](auto& range) { if (is_sorted(range)) { std::swap(range.front(), range.back()); } };
  unsort(request.accessSubjects); // we do not want accidental successes
  unsort(request.referenceSubjects);

  ResolveDuplicatePseudonyms(request);

  EXPECT_TRUE(std::ranges::is_sorted(request.accessSubjects));
  EXPECT_TRUE(std::ranges::is_sorted(request.referenceSubjects));
}

} // namespace
