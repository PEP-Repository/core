#pragma once

#include <pep/crypto/Timestamp.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/serialization/Error.hpp>

namespace pep {

// See Messages.proto for description of versions.
enum class SignatureScheme {
  V3 = 2,
  V4 = 3
};


class Signatory {
private:
  X509CertificateChain certificateChain_;
  X509RootCertificates rootCAs_;

public:
  Signatory(X509CertificateChain certificateChain, X509RootCertificates rootCAs);

  const X509CertificateChain& certificateChain() const noexcept { return certificateChain_; }
  const X509RootCertificates& rootCAs() const noexcept { return rootCAs_; }

  std::string commonName() const;
  std::string organizationalUnit() const;
};


class Signature {
  friend class Serializer<Signature>;

private:
  std::string signature_;
  X509CertificateChain certificateChain_;
  SignatureScheme scheme_ = SignatureScheme::V4;
  Timestamp timestamp_;
  bool isLogCopy_ = false;

public:
  Signature(
      std::string signature,
      X509CertificateChain chain,
      SignatureScheme scheme,
      Timestamp timestamp,
      bool isLogCopy)
    : signature_(std::move(signature)),
      certificateChain_(std::move(chain)),
      scheme_(scheme),
      timestamp_(timestamp),
      isLogCopy_(isLogCopy) { }

  static Signature Make(
      std::string_view data,
      const X509Identity& identity,
      bool isLogCopy=false,
      SignatureScheme scheme=SignatureScheme::V4);

  const X509CertificateChain& certificateChain() const noexcept { return certificateChain_; }
  Timestamp timestamp() const { return timestamp_; }

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
