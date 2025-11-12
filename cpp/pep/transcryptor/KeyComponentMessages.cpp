#include <pep/transcryptor/KeyComponentMessages.hpp>

#include <pep/auth/EnrolledParty.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>

namespace pep {

KeyComponentResponse KeyComponentResponse::HandleRequest(
  const SignedKeyComponentRequest& signedRequest,
  const PseudonymTranslator& pseudonymTranslator,
  const DataTranslator& dataTranslator,
  const X509RootCertificates& rootCAs
) {
  signedRequest.validate(rootCAs);
  auto party = GetEnrolledParty(signedRequest.mSignature.mCertificateChain);
  LOG("handleKeyComponentRequest", debug) << "Handling SignedKeyComponentRequest for facilityType "
    << static_cast<unsigned>(party);
  if (party != EnrolledParty::User &&
    party != EnrolledParty::StorageFacility &&
    party != EnrolledParty::Transcryptor &&
    party != EnrolledParty::AccessManager &&
    party != EnrolledParty::RegistrationServer) {
    throw Error("KeyComponentRequest denied");
  }

  auto recipient = RecipientForCertificate(signedRequest.getLeafCertificate());
  KeyComponentResponse response;
  response.mPseudonymKeyComponent = pseudonymTranslator.generateKeyComponent(recipient);
  if (party == EnrolledParty::User || party == EnrolledParty::RegistrationServer) {
    response.mEncryptionKeyComponent = dataTranslator.generateKeyComponent(recipient);
  }
  return response;
}

}
