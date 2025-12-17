#include <pep/crypto/Signed.hpp>

namespace pep {

SignedBase::SignedBase(std::string data, const X509Identity& identity)
  : mData(std::move(data)), mSignature(Signature::Make(mData, identity)) {
}

void SignedBase::assertValid(
  const X509RootCertificates& rootCAs,
  std::optional<std::string> expectedCommonName,
  std::chrono::seconds timestampLeeway) const {
  mSignature.assertValid(
    mData,
    rootCAs,
    expectedCommonName,
    timestampLeeway
  );
}

std::string SignedBase::getLeafCertificateCommonName() const {
  return mSignature.getLeafCertificateCommonName();
}

std::string SignedBase::getLeafCertificateOrganizationalUnit() const {
  return mSignature.getLeafCertificateOrganizationalUnit();
}

X509Certificate SignedBase::getLeafCertificate() const {
  return mSignature.getLeafCertificate();
}

MessageSigner::MessageSigner(std::shared_ptr<const X509Identity> signingIdentity) noexcept
  : mSigningIdentity(std::move(signingIdentity)) {
}

std::shared_ptr<const X509Identity> MessageSigner::getSigningIdentity(bool require) const {
  auto result = mSigningIdentity;
  if (require && result == nullptr) {
    throw std::runtime_error("No signing identity available");
  }
  return result;
}

void MessageSigner::setSigningIdentity(std::shared_ptr<const X509Identity> signingIdentity) noexcept {
  mSigningIdentity = std::move(signingIdentity);
}

}
