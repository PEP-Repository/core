#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/crypto/Signature.hpp>
#include <pep/crypto/X509Certificate.hpp>

namespace pep {

class LocalPseudonyms {
public:
  EncryptedLocalPseudonym mAccessManager;
  EncryptedLocalPseudonym mStorageFacility;
  PolymorphicPseudonym mPolymorphic;
  std::optional<EncryptedLocalPseudonym> mAccessGroup{};

  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
};

class Ticket2 {
public:
  Timestamp mTimestamp{/*zero*/};
  std::vector<std::string> mModes;
  std::vector<LocalPseudonyms> mPseudonyms;
  std::vector<std::string> mColumns;
  std::string mUserGroup;

  bool hasMode(const std::string& mode) const;
  std::vector<PolymorphicPseudonym> getPolymorphicPseudonyms() const;
};

class SignedTicket2 {
public:
  SignedTicket2() = default;
  SignedTicket2(
    Ticket2 ticket,
    X509CertificateChain chain,
    const AsymmetricKey& privateKey);
  SignedTicket2(
    std::optional<Signature> mSignature,
    std::optional<Signature> mTranscryptorSignature,
    std::string mData)
    : mSignature(std::move(mSignature)),
    mTranscryptorSignature(std::move(mTranscryptorSignature)),
    mData(std::move(mData)) { }

  std::optional<Signature> mSignature;
  std::optional<Signature> mTranscryptorSignature;
  std::string mData;

  Ticket2 openWithoutCheckingSignature() const;

  Ticket2 open(const X509RootCertificates& rootCAs,
    const std::string& accessGroup,
    const std::optional<std::string>& accessMode = std::nullopt) const;

  Ticket2 openForLogging(const X509RootCertificates& rootCAs) const;
};

class SignedTicket2ValidityPeriodError : public DeserializableDerivedError<SignedTicket2ValidityPeriodError> {
public:
  explicit inline SignedTicket2ValidityPeriodError(const std::string& description)
    : DeserializableDerivedError<SignedTicket2ValidityPeriodError>(description) { }
};

class TicketRequest2 {
public:
  std::vector<std::string> mModes;
  std::vector<std::string> mParticipantGroups;
  std::vector<PolymorphicPseudonym> mPolymorphicPseudonyms;
  std::vector<std::string> mColumnGroups;
  std::vector<std::string> mColumns;
  bool mRequestIndexedTicket = false;
  bool mIncludeUserGroupPseudonyms = false;
};

class SignedTicketRequest2 {
public:
  SignedTicketRequest2() = default;
  SignedTicketRequest2(TicketRequest2 ticketRequest,
    const X509CertificateChain& chain,
    const AsymmetricKey& privateKey);
  SignedTicketRequest2(
    std::optional<Signature> mSignature,
    std::optional<Signature> mLogSignature,
    std::string mData)
    : mSignature(std::move(mSignature)),
    mLogSignature(std::move(mLogSignature)),
    mData(std::move(mData)) { }

  TicketRequest2 openAsAccessManager(const X509RootCertificates& rootCAs);
  TicketRequest2 openAsTranscryptor(const X509RootCertificates& rootCAs);

  std::optional<Signature> mSignature;
  std::optional<Signature> mLogSignature;
  std::string mData;
};

}
