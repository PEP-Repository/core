#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace pep::tests {

inline PolymorphicPseudonym SingleRandomPolymorphicPseudonym() {
  return PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "arbitrary_identifier");
}

inline LocalPseudonyms SingleRandomLocalPseudonym() {
  const auto publicKey = ElgamalPublicKey::Random();
  const auto localPseudonym = LocalPseudonym::Random();
  return LocalPseudonyms{
    .accessManager = localPseudonym.encrypt(publicKey),
    .storageFacility = localPseudonym.encrypt(publicKey),
    .polymorphic = SingleRandomPolymorphicPseudonym(),
    .accessGroup = localPseudonym.encrypt(publicKey)
  };
}

template <std::size_t N>
std::array<PolymorphicPseudonym, N> RandomPolymorphicPseudonyms() {
  std::array<PolymorphicPseudonym, N> result{};
  std::ranges::generate(result, SingleRandomPolymorphicPseudonym);
  return result;
}

template <std::size_t N>
std::array<LocalPseudonyms, N> RandomLocalPseudonyms() {
  std::array<LocalPseudonyms, N> result{};
  std::ranges::generate(result, SingleRandomLocalPseudonym);
  return result;
}

} // namespace pep::tests
