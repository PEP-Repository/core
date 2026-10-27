#include <gtest/gtest.h>

#include <pep/ticketing/tests/LocalPseudonyms.test.hpp>
#include <pep/transcryptor/Transcryptor.hpp>

#include <vector>

using namespace pep;
using namespace pep::tests;

namespace {

using Vector = std::vector<LocalPseudonyms>;

TEST(ComputePseudonymHash, DependsOnBothLists) {
  const auto both = Generate<2>(SingleRandomLocalPseudonym);
  const auto reverse = Vector{both.back(), both.front()};
  const auto only_first = Vector{both.front()};
  const auto only_second = Vector{both.back()};
  const auto none = Vector{};

  // Stable output (here we add an extra pair of braces to make copies)
  EXPECT_EQ(ComputePseudonymHash(only_first, only_second), ComputePseudonymHash({only_first}, {only_second}));
  EXPECT_EQ(ComputePseudonymHash(both, none), ComputePseudonymHash({both}, {none}));
  EXPECT_EQ(ComputePseudonymHash(none, none), ComputePseudonymHash({none}, {none})); // edge case

  // Depends on which items are in each list
  EXPECT_NE(ComputePseudonymHash(only_first, none), ComputePseudonymHash(both, none));
  EXPECT_NE(ComputePseudonymHash(none, both), ComputePseudonymHash(none, only_second));

  // Depends on how the lists are split
  EXPECT_NE(ComputePseudonymHash(only_first, only_second), ComputePseudonymHash(both, none));
  EXPECT_NE(ComputePseudonymHash(only_first, only_second), ComputePseudonymHash(none, both));

  // Depends on order of items
  EXPECT_NE(ComputePseudonymHash(only_first, only_second), ComputePseudonymHash(only_second, only_first));
  EXPECT_NE(ComputePseudonymHash(both, none), ComputePseudonymHash(reverse, none));
}

} // namespace
