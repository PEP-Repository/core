#include <pep/auth/Signed.hpp>

namespace pep {

SignedBase::SignedBase(std::string data, const X509Identity& identity)
  : data_(std::move(data)), signature_(Signature::Make(data_, identity)) {
}

Signatory SignedBase::validate(const X509RootCertificates& rootCAs) const {
  return signature_.validate(
    data_,
    rootCAs,
    std::nullopt,
    std::chrono::hours{1}
  );
}

MessageSigner::MessageSigner(std::shared_ptr<const X509Identity> signingIdentity) noexcept
  : signingIdentity_(std::move(signingIdentity)) {
}

std::shared_ptr<const X509Identity> MessageSigner::getSigningIdentity(bool require) const {
  auto result = signingIdentity_;
  if (require && result == nullptr) {
    throw std::runtime_error("No signing identity available");
  }
  return result;
}

void MessageSigner::setSigningIdentity(std::shared_ptr<const X509Identity> signingIdentity) noexcept {
  signingIdentity_ = std::move(signingIdentity);
}

}
