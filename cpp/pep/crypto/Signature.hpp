#pragma once

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/crypto/X509Certificate.hpp>

#include <optional>
#include <string>

namespace pep {

// See Messages.proto for description of versions.
enum SignatureScheme {
  SIGNATURE_SCHEME_V3 = 2,
  SIGNATURE_SCHEME_V4 = 3
};


class Signature {
 public:
  std::string mSignature;
  X509CertificateChain mCertificateChain;
  SignatureScheme mScheme = SIGNATURE_SCHEME_V4;
  Timestamp mTimestamp;
  bool mIsLogCopy = false;

  Signature(
      std::string signature,
      X509CertificateChain chain,
      SignatureScheme scheme,
      Timestamp timestamp,
      bool isLogCopy)
    : mSignature(std::move(signature)),
      mCertificateChain(std::move(chain)),
      mScheme(scheme),
      mTimestamp(timestamp),
      mIsLogCopy(isLogCopy) { }

  static Signature Make(
      std::string_view data,
      const X509Identity& identity,
      bool isLogCopy=false,
      SignatureScheme scheme=SIGNATURE_SCHEME_V4);

  void assertValid(
      std::string_view data,
      const X509RootCertificates& rootCAs,
      std::optional<std::string> expectedCommonName,
      std::chrono::seconds timestampLeeway,
      bool expectLogCopy=false) const;

  template<typename T>
  T open(
      std::string_view data,
      const X509RootCertificates& rootCAs,
      std::optional<std::string> expectedCommonName=std::nullopt,
      std::chrono::seconds timestampLeeway = std::chrono::hours{1}) const {
    // This function checks whether the signature is valid and throws
    // a network-portable Error exception if it isn't.
    assertValid(data, rootCAs, expectedCommonName, timestampLeeway);
    return Serialization::FromString<T>(data);
  }

  std::string getLeafCertificateCommonName() const;
  std::string getLeafCertificateOrganizationalUnit() const;
  X509Certificate getLeafCertificate() const;
};

class SignatureValidityPeriodError : public DeserializableDerivedError<SignatureValidityPeriodError> {
public:
  explicit inline SignatureValidityPeriodError(const std::string& description)
    : DeserializableDerivedError<SignatureValidityPeriodError>(description) { }
};

}
