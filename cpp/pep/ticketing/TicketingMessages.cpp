#include <pep/ticketing/TicketingSerializers.hpp>

#include <pep/utils/MiscUtil.hpp>

using namespace std::literals;

namespace pep {

void LocalPseudonyms::ensurePacked() const {
  mAccessManager.ensurePacked();
  mStorageFacility.ensurePacked();
  mPolymorphic.ensurePacked();
  if (mAccessGroup)
    mAccessGroup->ensurePacked();
}

bool Ticket2::hasMode(const std::string& mode) const {
  // Check if the ticket explicitly includes the specified mode
  if (std::find(mModes.begin(), mModes.end(), mode) != mModes.end()) {
    return true;
  }
  // The "read-meta" mode is implicitly covered if the ticket includes "read" access
  if (mode == "read-meta") {
    return this->hasMode("read");
  }
  // The "write" mode is implicitly covered if the ticket includes "write-meta" access
  if (mode == "write") {
    return this->hasMode("write-meta");
  }
  // Mode not covered by the ticket
  return false;
}

std::vector<PolymorphicPseudonym> Ticket2::getPolymorphicPseudonyms() const {
  std::vector<PolymorphicPseudonym> ret;
  ret.reserve(mPseudonyms.size());
  for (const auto& p : mPseudonyms)
    ret.push_back(p.mPolymorphic);
  return ret;
}

Ticket2 SignedTicket2::openWithoutCheckingSignature() const {
  return Serialization::FromString<Ticket2>(mData);
}

Ticket2 SignedTicket2::open(const X509RootCertificates& rootCAs,
  const std::string& userGroup, const std::optional<std::string>& accessMode) const {
  if (!mSignature)
    throw Error("AccessManager signature is missing");
  if (!mTranscryptorSignature)
    throw Error("Transcryptor signature is missing");

  try {
    mSignature->assertValid(
      mData,
      rootCAs,
      std::string("AccessManager"),
      std::chrono::days{1},
      false
    );
    mTranscryptorSignature->assertValid(
      mData,
      rootCAs,
      std::string("Transcryptor"),
      std::chrono::days{1},
      false
    );
  }
  catch (const SignatureValidityPeriodError& sig) {
    throw SignedTicket2ValidityPeriodError(sig.mDescription);
  }

  auto ticket = Serialization::FromString<Ticket2>(mData);
  if (ticket.mUserGroup != userGroup)
    throw Error("Ticket issued for different user group");
  if (accessMode.has_value()) {
    if (!ticket.hasMode(*accessMode)) {
      throw Error("Ticket does not grant required " + *accessMode + " access");
    }
  }
  return ticket;
}

Ticket2 SignedTicket2::openForLogging(const X509RootCertificates& rootCAs) const {
  if (!mSignature)
    throw Error("AccessManager signature is missing");
  if (mTranscryptorSignature)
    throw Error("Transcryptor signature should not be set");

  mSignature->assertValid(
    mData,
    rootCAs,
    std::string("AccessManager"),
    std::chrono::days{1},
    false
  );

  auto ticket = Serialization::FromString<Ticket2>(mData);
  return ticket;
}

Signed<Ticket2>::Signed(Ticket2 ticket, X509CertificateChain chain,
  const AsymmetricKey& privateKey) {
  auto data = Serialization::ToString(std::move(ticket));
  mSignature = Signature::create(data, chain, privateKey);
  mData = std::move(data);
}

Signed<TicketRequest2>::Signed(TicketRequest2 ticketRequest,
  const X509CertificateChain& chain,
  const AsymmetricKey& privateKey) {
  mData = Serialization::ToString(std::move(ticketRequest));
  mSignature = Signature::create(mData, chain, privateKey);
  mLogSignature = Signature::create(mData, chain, privateKey, true);
}

TicketRequest2 Signed<TicketRequest2>::openAsAccessManager(
  const X509RootCertificates& rootCAs) {
  if (!mSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature");
  if (!mLogSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature for logger");

  // Check signatures separately
  mSignature->assertValid(mData, rootCAs, std::nullopt, 1h, false);
  mLogSignature->assertValid(mData, rootCAs, std::nullopt, 1h, true);

  // Check signatures are similar enough
  auto diff = Abs(mSignature->mTimestamp - mLogSignature->mTimestamp);
  if (diff > 1min)
    throw Error("Invalid SignedTicketRequest2: timestamps "
      "of signatures too far apart");

  // TODO instead it's better to check the public keys are the same.  (Then
  // don't have to check all the other fields of the certificate that might
  // become relevant).
  if (mSignature->getLeafCertificateOrganizationalUnit()
    != mLogSignature->getLeafCertificateOrganizationalUnit())
    throw Error("Invalid SignedTicketRequest2: organizational "
      "units of signatures do not match");

  return Serialization::FromString<TicketRequest2>(mData);
}

TicketRequest2 Signed<TicketRequest2>::openAsTranscryptor(
  const X509RootCertificates& rootCAs) {
  if (mSignature)
    throw Error("Invalid SignedTicketRequest2: signature for AM shouldn't be set");
  if (!mLogSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature for logger");

  mLogSignature->assertValid(mData, rootCAs, std::nullopt, 1h, true);

  return Serialization::FromString<TicketRequest2>(mData);
}

}
