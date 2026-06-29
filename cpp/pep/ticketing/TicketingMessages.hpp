#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/auth/Signed.hpp>

namespace pep {

struct LocalPseudonyms {
  EncryptedLocalPseudonym accessManager;
  EncryptedLocalPseudonym storageFacility;
  PolymorphicPseudonym polymorphic;
  std::optional<EncryptedLocalPseudonym> accessGroup{};

  // Ensures the underlying CurvePoint's are pre-packed for serialization.
  // See CurvePoint::ensurePacked().
  void ensurePacked() const;
};

/// Utility function to convert a vector of LocalPseudonyms to a vector of PolymorphicPseudonyms
std::vector<PolymorphicPseudonym> GetPolymorphicPseudonyms(const std::vector<LocalPseudonyms>&);

struct Ticket2 {
  Timestamp timestamp{/*zero*/};
  std::vector<std::string> modes;
  std::vector<LocalPseudonyms> accessSubjects;  ///< identifiers for subjects that will be accessed
  std::vector<std::string> columns;
  std::string userGroup;

  bool hasMode(const std::string& mode) const;
};

template <>
class Signed<Ticket2> {
  friend class Serializer<Signed<Ticket2>>;

private:
  std::optional<Signature> signature_;
  std::optional<Signature> transcryptorSignature_;
  std::string data_;

public:
  Signed() = default;
  Signed(
    Ticket2 ticket,
    const X509Identity& identity);
  Signed(
    std::optional<Signature> signature_,
    std::optional<Signature> transcryptorSignature_,
    std::string data_)
    : signature_(std::move(signature_)),
    transcryptorSignature_(std::move(transcryptorSignature_)),
    data_(std::move(data_)) { }

  void addTranscryptorSignature(Signature signature);

  Ticket2 openWithoutCheckingSignature() const;

  Ticket2 open(const X509RootCertificates& rootCAs,
    const std::string& accessGroup,
    const std::optional<std::string>& accessMode = std::nullopt) const;

  Ticket2 openForLogging(const X509RootCertificates& rootCAs, std::string& serialized) const;
};

class SignedTicket2ValidityPeriodError : public DeserializableDerivedError<SignedTicket2ValidityPeriodError> {
public:
  explicit inline SignedTicket2ValidityPeriodError(const std::string& description)
    : DeserializableDerivedError<SignedTicket2ValidityPeriodError>(description) { }
};

struct ClientSideTicketRequest2 {
  std::vector<std::string> modes;
  std::vector<std::string> participantGroups;
  std::vector<PolymorphicPseudonym> accessSubjects;
  std::vector<std::string> columnGroups;
  std::vector<std::string> columns;
  bool includeUserGroupPseudonyms = false;
};

struct TicketRequest2 : public ClientSideTicketRequest2 {
  bool requestIndexedTicket{};
};

template <>
class Signed<TicketRequest2> {
  friend class Serializer<Signed<TicketRequest2>>;

private:
  std::optional<Signature> signature_;
  std::optional<Signature> logSignature_;
  std::string data_;

public:
  Signed(TicketRequest2 ticketRequest,
    const X509Identity& identity);
  Signed(
    std::optional<Signature> signature_,
    std::optional<Signature> logSignature_,
    std::string data_)
    : signature_(std::move(signature_)),
    logSignature_(std::move(logSignature_)),
    data_(std::move(data_)) { }

  const std::optional<Signature>& logSignature() const noexcept { return logSignature_; }
  Signature extractSignature();

  Certified<TicketRequest2> openAsAccessManager(const X509RootCertificates& rootCAs);
  Certified<TicketRequest2> openAsTranscryptor(const X509RootCertificates& rootCAs);
};

using SignedTicket2 = Signed<Ticket2>;
using SignedTicketRequest2 = Signed<TicketRequest2>;

}
