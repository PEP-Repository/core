#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace pep::tests {

inline PolymorphicPseudonym SingleRandomPolymorphicPseudonym() {
  return PolymorphicPseudonym::FromIdentifier(ElgamalPublicKey::Random(), "arbitrary_identifier");
}

inline LocalPseudonyms SingleRandomLocalPseudonym() {
  const auto publicKey = ElgamalPublicKey::Random();
  const auto localPseudonym = LocalPseudonym::Random();
  return LocalPseudonyms{.accessManager = localPseudonym.encrypt(publicKey),
                         .storageFacility = localPseudonym.encrypt(publicKey),
                         .polymorphic = SingleRandomPolymorphicPseudonym(),
                         .accessGroup = localPseudonym.encrypt(publicKey)};
}

template <std::size_t N, typename TGenerator>
std::array<std::invoke_result_t<TGenerator>, N> Generate(TGenerator generator) {
  std::array<std::invoke_result_t<TGenerator>, N> result{};
  std::ranges::generate(result, generator);
  return result;
}

} // namespace pep::tests
