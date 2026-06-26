#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

struct RekeyRequest {
  std::vector<EncryptedKey> keys;
  X509CertificateChain clientCertificateChain;
};

struct RekeyResponse {
  std::vector<EncryptedKey> keys;
};

struct TranscryptorRequestEntry {
  PolymorphicPseudonym polymorphic;

  // Partially translated
  EncryptedLocalPseudonym accessManager;
  EncryptedLocalPseudonym storageFacility;
  EncryptedLocalPseudonym transcryptor;
  std::optional<EncryptedLocalPseudonym> userGroup;

  RskProof accessManagerProof;
  RskProof storageFacilityProof;
  RskProof transcryptorProof;
  std::optional<RskProof> userGroupProof;

  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
};

struct TranscryptorRequestEntries {
  std::vector<TranscryptorRequestEntry> entries;
};

struct TranscryptorRequest {
  SignedTicketRequest2 request;
};

struct TranscryptorResponse {
  std::vector<LocalPseudonyms> entries;
  std::string id;
};

struct LogIssuedTicketRequest {
  SignedTicket2 ticket;
  std::string id;
};

struct LogIssuedTicketResponse {
  Signature signature;
};

}
