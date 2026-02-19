#include <pep/key-components/KeyComponentMessages.hpp>

#include <pep/auth/EnrolledParty.hpp>
#include <pep/morphing/RepoRecipient.hpp>

namespace pep {

KeyComponentResponse KeyComponentResponse::HandleRequest(
  const SignedKeyComponentRequest& signedRequest,
  const PseudonymTranslator& pseudonymTranslator,
  const DataTranslator& dataTranslator,
  const X509RootCertificates& rootCAs
) {
  signedRequest.validate(rootCAs);
  auto party = GetEnrolledParty(signedRequest.mSignature.mCertificateChain);
  if (!party.has_value()) {
    throw Error("KeyComponentRequest denied");
  }

  auto recipient = RecipientForCertificate(signedRequest.getLeafCertificate());
  KeyComponentResponse response;
  response.mPseudonymKeyComponent = pseudonymTranslator.generateKeyComponent(recipient);
  if (HasDataAccess(*party)) {
    response.mEncryptionKeyComponent = dataTranslator.generateKeyComponent(recipient);
  }
  return response;
}

}
