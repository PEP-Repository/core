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
  const auto randomEncryptedLocalPseudonym = [publicKey = ElgamalPublicKey::Random()] {
    return LocalPseudonym::Random().encrypt(publicKey);
  };
  return LocalPseudonyms{.accessManager = randomEncryptedLocalPseudonym(),
                         .storageFacility = randomEncryptedLocalPseudonym(),
                         .polymorphic = SingleRandomPolymorphicPseudonym(),
                         .accessGroup = randomEncryptedLocalPseudonym()};
}

template <std::size_t N, typename TGenerator>
std::array<std::invoke_result_t<TGenerator>, N> Generate(TGenerator generator) {
  std::array<std::invoke_result_t<TGenerator>, N> result{};
  std::ranges::generate(result, generator);
  return result;
}

} // namespace pep::tests
