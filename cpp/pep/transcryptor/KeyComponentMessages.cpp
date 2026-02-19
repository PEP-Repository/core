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
  auto signatory = signedRequest.validate(rootCAs);
  auto party = GetEnrolledParty(signatory.certificateChain());
  if (!party.has_value()) {
    throw Error("KeyComponentRequest denied");
  }

  auto recipient = RecipientForCertificate(signatory.certificateChain().leaf());
  KeyComponentResponse response;
  response.mPseudonymKeyComponent = pseudonymTranslator.generateKeyComponent(recipient);
  if (HasDataAccess(*party)) {
    response.mEncryptionKeyComponent = dataTranslator.generateKeyComponent(recipient);
  }
  return response;
}

}
