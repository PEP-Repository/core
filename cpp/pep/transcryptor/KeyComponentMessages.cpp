#include <pep/transcryptor/KeyComponentMessages.hpp>

#include <pep/auth/FacilityType.hpp>
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
  auto facilityType = GetFacilityType(signedRequest.mSignature.mCertificateChain);
  LOG("handleKeyComponentRequest", debug) << "Handling SignedKeyComponentRequest for facilityType "
    << static_cast<unsigned>(facilityType);
  if (facilityType != FacilityType::User &&
    facilityType != FacilityType::StorageFacility &&
    facilityType != FacilityType::Transcryptor &&
    facilityType != FacilityType::AccessManager &&
    facilityType != FacilityType::RegistrationServer) {
    throw Error("KeyComponentRequest denied");
  }

  auto recipient = RecipientForCertificate(signedRequest.getLeafCertificate());
  KeyComponentResponse response;
  response.mPseudonymKeyComponent = pseudonymTranslator.generateKeyComponent(recipient);
  if (facilityType == FacilityType::User || facilityType == FacilityType::RegistrationServer) {
    response.mEncryptionKeyComponent = dataTranslator.generateKeyComponent(recipient);
  }
  return response;
}

}
