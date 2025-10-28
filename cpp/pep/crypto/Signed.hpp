#pragma once

#include <pep/serialization/Serialization.hpp>
#include <pep/crypto/Signature.hpp>
#include <pep/crypto/X509Certificate.hpp>

namespace pep {

class SignedBase {
public:
  std::string mData;
  Signature mSignature;

protected:
  void assertValid(
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName,
    std::chrono::seconds timestampLeeway) const;

public:
  SignedBase() = default;
  SignedBase(
    std::string data,
    const X509Identity& identity);

  SignedBase(
    std::string data,
    Signature signature)
    : mData(std::move(data)),
    mSignature(std::move(signature)) { }

  std::string getLeafCertificateCommonName() const;
  std::string getLeafCertificateOrganizationalUnit() const;
  X509Certificate getLeafCertificate() const;
};

template<typename T>
class Signed : public SignedBase {
public:
  Signed() = default; // TODO get rid of default constructor
  Signed(std::string data, Signature signature)
    : SignedBase(std::move(data), std::move(signature)) { }
  Signed(T o, const X509Identity& identity) :
    SignedBase(Serialization::ToString(std::move(o)), identity) { }

  [[nodiscard]] T open(
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName = std::nullopt,
    std::chrono::seconds timestampLeeway = std::chrono::hours{1}) const {
    return mSignature.open<T>(mData, rootCAs, expectedCommonName, timestampLeeway);
  }

  void validate(
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName = std::nullopt,
    std::chrono::seconds timestampLeeway = std::chrono::hours{1}) const {
    mSignature.assertValid(mData, rootCAs, expectedCommonName, timestampLeeway);
  }

  T openWithoutCheckingSignature() const {
    return Serialization::FromString<T>(mData);
  }

};

class MessageSigner {
private:
  std::shared_ptr<const X509Identity> mSigningIdentity;

protected:
  std::shared_ptr<const X509Identity> getSigningIdentity(bool require = true) const;

public:
  explicit MessageSigner(std::shared_ptr<const X509Identity> signingIdentity = nullptr) noexcept;

  void setSigningIdentity(std::shared_ptr<const X509Identity> signingIdentity) noexcept;

  template <typename T>
  Signed<T> sign(T message) const {
    return Signed<T>(std::move(message), *this->getSigningIdentity());
  }
};

template <typename T>
struct NormalizedTypeNamer<Signed<T>> : public BasicNormalizedTypeNamer {
  static inline std::string GetTypeName() { return "Signed" + GetNormalizedTypeName<T>(); }
};

}
