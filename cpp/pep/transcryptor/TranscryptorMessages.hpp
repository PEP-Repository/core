#pragma once

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/ticketing/TicketingMessages.hpp>

namespace pep {

class RekeyRequest {
public:
  std::vector<EncryptedKey> mKeys;
  X509CertificateChain mClientCertificateChain;
};

class RekeyResponse {
public:
  std::vector<EncryptedKey> mKeys;
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
    : mPolymorphic(pp),
    accessManager_(am),
    mStorageFacility(sf),
    mTranscryptor(ts),
    userGroup_(ug),
    mAccessManagerProof(amProof),
    mStorageFacilityProof(sfProof),
    mTranscryptorProof(tsProof),
    mUserGroupProof(ugProof) { }

  PolymorphicPseudonym mPolymorphic;

  // Partially translated
  EncryptedLocalPseudonym accessManager_;
  EncryptedLocalPseudonym mStorageFacility;
  EncryptedLocalPseudonym mTranscryptor;
  std::optional<EncryptedLocalPseudonym> userGroup_;

  RskProof mAccessManagerProof;
  RskProof mStorageFacilityProof;
  RskProof mTranscryptorProof;
  std::optional<RskProof> mUserGroupProof;

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
  SignedTicketRequest2 mRequest;
};

class TranscryptorResponse {
public:
  TranscryptorResponse() = default;
  std::vector<LocalPseudonyms> entries_;
  std::string mId;
};

class LogIssuedTicketRequest {
public:
  SignedTicket2 ticket_;
  std::string mId;
};

class LogIssuedTicketResponse {
public:
  LogIssuedTicketResponse(Signature sig) : mSignature(std::move(sig)) { }
  Signature mSignature;
};

}
