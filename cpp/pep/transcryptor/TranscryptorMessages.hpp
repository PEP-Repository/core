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
    const ReshuffleRekeyProof& amProof,
    const ReshuffleRekeyProof& sfProof,
    const ReshuffleRekeyProof& tsProof,
    const std::optional<ReshuffleRekeyProof>& ugProof)
    : mPolymorphic(pp),
    mAccessManager(am),
    mStorageFacility(sf),
    mTranscryptor(ts),
    mUserGroup(ug),
    mAccessManagerProof(amProof),
    mStorageFacilityProof(sfProof),
    mTranscryptorProof(tsProof),
    mUserGroupProof(ugProof) { }

  PolymorphicPseudonym mPolymorphic;

  // Partially translated
  EncryptedLocalPseudonym mAccessManager;
  EncryptedLocalPseudonym mStorageFacility;
  EncryptedLocalPseudonym mTranscryptor;
  std::optional<EncryptedLocalPseudonym> mUserGroup;

  ReshuffleRekeyProof mAccessManagerProof;
  ReshuffleRekeyProof mStorageFacilityProof;
  ReshuffleRekeyProof mTranscryptorProof;
  std::optional<ReshuffleRekeyProof> mUserGroupProof;

  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
};

class TranscryptorRequestEntries {
public:
  std::vector<TranscryptorRequestEntry> mEntries;
};

class TranscryptorRequest {
public:
  SignedTicketRequest2 mRequest;
};

class TranscryptorResponse {
public:
  TranscryptorResponse() = default;
  std::vector<LocalPseudonyms> mEntries;
  std::string mId;
};

class LogIssuedTicketRequest {
public:
  SignedTicket2 mTicket;
  std::string mId;
};

class LogIssuedTicketResponse {
public:
  LogIssuedTicketResponse(Signature sig) : mSignature(std::move(sig)) { }
  Signature mSignature;
};

}
