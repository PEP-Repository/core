#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

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

} // namespace pep::tests
