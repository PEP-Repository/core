#include <pep/auth/Signed.hpp>

namespace pep {

SignedBase::SignedBase(std::string data, const X509Identity& identity)
  : mData(std::move(data)), mSignature(Signature::Make(mData, identity)) {
}

Signatory SignedBase::validate(const X509RootCertificates& rootCAs) const {
  return mSignature.validate(
    mData,
    rootCAs,
    std::nullopt,
    std::chrono::hours{1}
  );
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
