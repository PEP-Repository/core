#pragma once

#include <pep/crypto/Timestamp.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/serialization/Error.hpp>

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
