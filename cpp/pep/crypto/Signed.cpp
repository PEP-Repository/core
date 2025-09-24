#include <pep/crypto/Signed.hpp>

namespace pep {

SignedBase::SignedBase(std::string data, X509CertificateChain chain,
  const AsymmetricKey& privateKey) : mData(std::move(data)) {
  mSignature = Signature::create(mData, chain, privateKey);
}

void SignedBase::assertValid(
  const X509RootCertificates& rootCAs,
  std::optional<std::string> expectedCommonName,
  uint64_t timestampLeewaySeconds) const {
  mSignature.assertValid(
    mData,
    rootCAs,
    expectedCommonName,
    timestampLeewaySeconds
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

}
