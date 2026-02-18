#pragma once

#include <pep/auth/Signature.hpp>
#include <pep/serialization/Serialization.hpp>

namespace pep {

class SignedBase {
public: // You should have no business accessing these unless you're (de)serializing
  std::string mData;
  Signature mSignature;

public:
  Signatory validate(
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName = std::nullopt) const;

protected:
  SignedBase(
    std::string data,
    const X509Identity& identity);

  SignedBase(
    std::string data,
    Signature signature)
    : mData(std::move(data)),
    mSignature(std::move(signature)) { }

  template <typename T>
  T deserializeAs() const {
    return Serialization::FromString<T>(mData);
  }
};

template <typename T>
struct Certified {
  Signatory signatory;
  T message;
};

template<typename T>
class Signed : public SignedBase {
public:
  Signed(std::string data, Signature signature)
    : SignedBase(std::move(data), std::move(signature)) { }
  Signed(T o, const X509Identity& identity) :
    SignedBase(Serialization::ToString(std::move(o)), identity) { }

  [[nodiscard]] Certified<T> open(
    const X509RootCertificates& rootCAs) const {
    auto signatory = this->validate(rootCAs, std::nullopt);
    return Certified<T>{
      .signatory = std::move(signatory),
      .message = this->openWithoutCheckingSignature(),
    };
  }

  T openWithoutCheckingSignature() const {
    return this->deserializeAs<T>();
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
