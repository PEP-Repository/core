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


class Signatory {
private:
  X509CertificateChain mCertificateChain;
  X509RootCertificates mRootCAs;

public:
  Signatory(X509CertificateChain certificateChain, X509RootCertificates rootCAs);

  const X509CertificateChain& certificateChain() const noexcept { return mCertificateChain; }
  const X509RootCertificates& rootCAs() const noexcept { return mRootCAs; }

  std::string commonName() const;
  std::string organizationalUnit() const;
};


class Signature {
  friend class Serializer<Signature>;

private:
  std::string mSignature;
  X509CertificateChain mCertificateChain;
  SignatureScheme mScheme = SIGNATURE_SCHEME_V4;
  Timestamp mTimestamp;
  bool mIsLogCopy = false;

public:
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

  const X509CertificateChain& certificateChain() const noexcept { return mCertificateChain; }
  Timestamp timestamp() const { return mTimestamp; }

  Signatory validate(
      std::string_view data,
      const X509RootCertificates& rootCAs,
      std::optional<std::string> expectedCommonName,
      std::chrono::seconds timestampLeeway,
      bool expectLogCopy=false) const;
};

class SignatureValidityPeriodError : public DeserializableDerivedError<SignatureValidityPeriodError> {
public:
  explicit inline SignatureValidityPeriodError(const std::string& description)
    : DeserializableDerivedError<SignatureValidityPeriodError>(description) { }
};

}
