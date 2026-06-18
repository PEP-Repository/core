#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

class RekeyRequest {
public:
  std::vector<EncryptedKey> keys_;
  X509CertificateChain clientCertificateChain_;
};

class RekeyResponse {
public:
  std::vector<EncryptedKey> keys_;
};

class TranscryptorRequestEntry {
public:
  TranscryptorRequestEntry() = default;
  TranscryptorRequestEntry(
    const PolymorphicPseudonym& pp,
    const EncryptedLocalPseudonym& am,
    const EncryptedLocalPseudonym& sf,
    const EncryptedLocalPseudonym& ts,
    const std::optional<EncryptedLocalPseudonym>& ug,
    const RskProof& amProof,
    const RskProof& sfProof,
    const RskProof& tsProof,
    const std::optional<RskProof>& ugProof)
    : polymorphic_(pp),
    accessManager_(am),
    storageFacility_(sf),
    transcryptor_(ts),
    userGroup_(ug),
    accessManagerProof_(amProof),
    storageFacilityProof_(sfProof),
    transcryptorProof_(tsProof),
    userGroupProof_(ugProof) { }

  PolymorphicPseudonym polymorphic_;

  // Partially translated
  EncryptedLocalPseudonym accessManager_;
  EncryptedLocalPseudonym storageFacility_;
  EncryptedLocalPseudonym transcryptor_;
  std::optional<EncryptedLocalPseudonym> userGroup_;

  RskProof accessManagerProof_;
  RskProof storageFacilityProof_;
  RskProof transcryptorProof_;
  std::optional<RskProof> userGroupProof_;

  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
};

class TranscryptorRequestEntries {
public:
  std::vector<TranscryptorRequestEntry> entries_;
};

class TranscryptorRequest {
public:
  SignedTicketRequest2 request_;
};

class TranscryptorResponse {
public:
  TranscryptorResponse() = default;
  std::vector<LocalPseudonyms> entries_;
  std::string id_;
};

class LogIssuedTicketRequest {
public:
  SignedTicket2 ticket_;
  std::string id_;
};

class LogIssuedTicketResponse {
public:
  LogIssuedTicketResponse(Signature sig) : signature_(std::move(sig)) { }
  Signature signature_;
};

}
