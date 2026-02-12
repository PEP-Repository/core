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
  if (!party.has_value()) {
    throw Error("KeyComponentRequest denied");
  }

  auto recipient = RecipientForCertificate(signedRequest.getLeafCertificate());
  KeyComponentResponse response;
  response.mPseudonymEncryptionKeyComponent = pseudonymTranslator.generateKeyComponent(recipient);
  if (HasDataAccess(*party)) {
    response.mDataEncryptionKeyComponent = dataTranslator.generateKeyComponent(recipient);
  }
  return response;
}

}
