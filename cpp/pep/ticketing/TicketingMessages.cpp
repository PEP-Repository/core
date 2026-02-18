#include <pep/auth/ServerTraits.hpp>
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
    // A longer leeway is used for long downloads etc.
    mSignature->validate(
      mData,
      rootCAs,
      ServerTraits::AccessManager().userGroup(true),
      std::chrono::days{1},
      false
    );
    mTranscryptorSignature->validate(
      mData,
      rootCAs,
      ServerTraits::Transcryptor().userGroup(true),
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

  mSignature->validate(
    mData,
    rootCAs,
    ServerTraits::AccessManager().userGroup(true),
    std::chrono::days{1},
    false
  );

  auto ticket = Serialization::FromString<Ticket2>(mData);
  return ticket;
}

Signed<Ticket2>::Signed(Ticket2 ticket,
  const X509Identity& identity) {
  auto data = Serialization::ToString(std::move(ticket));
  mSignature = Signature::Make(data, identity);
  mData = std::move(data);
}

Signed<TicketRequest2>::Signed(TicketRequest2 ticketRequest,
  const X509Identity& identity) {
  mData = Serialization::ToString(std::move(ticketRequest));
  mSignature = Signature::Make(mData, identity);
  mLogSignature = Signature::Make(mData, identity, true);
}

Certified<TicketRequest2> Signed<TicketRequest2>::certifyForAccessManager(
  const X509RootCertificates& rootCAs) {
  if (!mSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature");
  if (!mLogSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature for logger");

  // Check signatures separately
  auto signatory = mSignature->validate(mData, rootCAs, std::nullopt, 1h, false);
  auto logSignatory = mLogSignature->validate(mData, rootCAs, std::nullopt, 1h, true);

  // Check signatures are similar enough
  auto diff = Abs(mSignature->timestamp() - mLogSignature->timestamp());
  if (diff > 1min)
    throw Error("Invalid SignedTicketRequest2: timestamps "
      "of signatures too far apart");

  // TODO instead it's better to check the public keys are the same.  (Then
  // don't have to check all the other fields of the certificate that might
  // become relevant).
  if (signatory.organizationalUnit() != logSignatory.organizationalUnit()) {
    throw Error("Invalid SignedTicketRequest2: organizational "
      "units of signatures do not match");
  }

  return Certified<TicketRequest2>{
    .signatory = std::move(signatory),
    .message = Serialization::FromString<TicketRequest2>(mData),
  };
}

Certified<TicketRequest2> Signed<TicketRequest2>::certifyForTranscryptor(
  const X509RootCertificates& rootCAs) {
  if (mSignature)
    throw Error("Invalid SignedTicketRequest2: signature for AM shouldn't be set");
  if (!mLogSignature)
    throw Error("Invalid SignedTicketRequest2: missing signature for logger");

  auto signatory = mLogSignature->validate(mData, rootCAs, std::nullopt, 1h, true);
  return Certified<TicketRequest2> {
    .signatory = std::move(signatory),
    .message = Serialization::FromString<TicketRequest2>(mData),
  };
}

}
