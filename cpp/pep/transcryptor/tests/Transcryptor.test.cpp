#include <gtest/gtest.h>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/transcryptor/Transcryptor.hpp>

#include <vector>

using namespace pep;

namespace {

using Vector = std::vector<LocalPseudonyms>;

template <std::size_t N>
std::array<LocalPseudonyms, N> RandomLocalPseudonyms() {
  std::array<LocalPseudonyms, N> array{};
  for(auto& pseudonyms: array) {
    const auto publicKey = ElgamalPublicKey::Random();
    const auto localPseudonym = LocalPseudonym::Random();
    pseudonyms = pep::LocalPseudonyms{
      .accessManager = localPseudonym.encrypt(publicKey),
      .storageFacility = localPseudonym.encrypt(publicKey),
      .polymorphic = PolymorphicPseudonym::FromIdentifier(publicKey, "arbitrary_identifier"),
      .accessGroup = localPseudonym.encrypt(publicKey)
    };
  }
  return array;
}

TEST(ComputePseudonymHash, DependsOnBothLists) {
  const auto both = RandomLocalPseudonyms<2>();
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
