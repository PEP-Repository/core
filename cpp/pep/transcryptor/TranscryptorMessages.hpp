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
    PolymorphicPseudonym pp,
    EncryptedLocalPseudonym am,
    EncryptedLocalPseudonym sf,
    EncryptedLocalPseudonym ts,
    std::optional<EncryptedLocalPseudonym> ug,
    RSKProof amProof,
    RSKProof sfProof,
    RSKProof tsProof,
    std::optional<RSKProof> ugProof)
    : mPolymorphic(std::move(pp)),
    mAccessManager(std::move(am)),
    mStorageFacility(std::move(sf)),
    mTranscryptor(std::move(ts)),
    mUserGroup(std::move(ug)),
    mAccessManagerProof(std::move(amProof)),
    mStorageFacilityProof(std::move(sfProof)),
    mTranscryptorProof(std::move(tsProof)),
    mUserGroupProof(std::move(ugProof)) { }

  PolymorphicPseudonym mPolymorphic;

  EncryptedLocalPseudonym mAccessManager;
  EncryptedLocalPseudonym mStorageFacility;
  EncryptedLocalPseudonym mTranscryptor;
  std::optional<EncryptedLocalPseudonym> mUserGroup;

  RSKProof mAccessManagerProof;
  RSKProof mStorageFacilityProof;
  RSKProof mTranscryptorProof;
  std::optional<RSKProof> mUserGroupProof;

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
  inline TranscryptorResponse() {}
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
  LogIssuedTicketResponse() = default;
  LogIssuedTicketResponse(Signature sig) : mSignature(std::move(sig)) { }
  Signature mSignature;
};

}
