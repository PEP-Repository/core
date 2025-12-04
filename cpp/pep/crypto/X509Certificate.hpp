#pragma once

#include <openssl/x509.h>
#include <openssl/asn1.h>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/utils/Configuration_fwd.hpp>

#include <list>
#include <chrono>
#include <optional>
#include <string>
#include <filesystem>

namespace pep {

class X509Certificate {
public:
  static constexpr const char* defaultSelfSignedCountryCode = "NL";
  static constexpr auto defaultSelfSignedValidity = std::chrono::hours(1);

  X509Certificate(const X509Certificate& other);
  X509Certificate(X509Certificate&& other) noexcept;
  ~X509Certificate() noexcept;
  X509Certificate& operator=(const X509Certificate& other);
  X509Certificate& operator=(X509Certificate&& other) noexcept;

  explicit X509Certificate(X509& cert) noexcept : mRaw(&cert) {} // Takes ownership
  [[nodiscard]] const X509& raw() const noexcept;

  AsymmetricKey getPublicKey() const;
  std::optional<std::string> getCommonName() const;
  std::optional<std::string> getOrganizationalUnit() const;
  std::optional<std::string> getIssuerCommonName() const;
  std::optional<unsigned long> pathLengthConstraint() const; //NOLINT(google-runtime-int)

  bool hasBasicConstraints() const;
  bool hasDigitalSignatureKeyUsage() const;
  bool hasTLSServerEKU() const;
  bool isSelfSigned() const;

  [[nodiscard]] bool verifySubjectKeyIdentifier() const;
  [[nodiscard]] bool verifyAuthorityKeyIdentifier(const X509Certificate& issuerCert) const;

  std::chrono::sys_seconds getNotBefore() const;
  std::chrono::sys_seconds getNotAfter() const;
  bool isCurrentTimeInValidityPeriod() const;

  std::string toPem() const;
  std::string toDer() const;

  static X509Certificate FromPem(const std::string& pem);
  static X509Certificate FromDer(const std::string& der);
  static X509Certificate MakeSelfSigned(const AsymmetricKeyPair& keys, std::string organization, std::string commonName, std::string countryCode = defaultSelfSignedCountryCode, std::chrono::seconds validityPeriod = defaultSelfSignedValidity);

  bool operator==(const X509Certificate& rhs) const;
  bool operator!=(const X509Certificate& rhs) const { return !(*this == rhs); }

 private:
  X509* mRaw = nullptr;

  [[nodiscard]] X509* rawPointer() const noexcept; // Some of our "const" methods require the X509 structure to be mutable
  std::optional<std::string> searchOIDinSubject(int nid) const;

  static X509Certificate MakeUnsigned(const AsymmetricKey& publicKey, const X509_NAME& subjectName, std::chrono::seconds validityPeriod);
  void sign(const AsymmetricKey& caPrivateKey, const X509_NAME& caName);

  friend class X509CertificateChain;
  friend class X509CertificateSigningRequest;
};


using X509Certificates = std::list<X509Certificate>;

X509Certificates X509CertificatesFromPem(const std::string& in);
std::string X509CertificatesToPem(const X509Certificates& certificates);


class X509RootCertificates {
private:
  X509Certificates mItems;

public:
  explicit X509RootCertificates(X509Certificates certificates);

  static X509RootCertificates FromFile(const std::filesystem::path& caCertFilePath);

  const X509Certificates& items() const noexcept { return mItems; }
};


class X509CertificateChain {
private:
  X509Certificates mCertificates;

public:
  X509CertificateChain(X509Certificates certificates);

  static X509CertificateChain MakeSelfSigned(AsymmetricKeyPair& keys, std::string organization, std::string commonName, std::string countryCode = X509Certificate::defaultSelfSignedCountryCode);

  const X509Certificate& leaf() const;
  X509CertificateChain& operator/=(X509Certificate leaf);

  [[nodiscard]] bool verify(const X509RootCertificates& rootCAs) const;
  bool isCurrentTimeInValidityPeriod() const;
  bool certifiesPrivateKey(const AsymmetricKey& privateKey) const;

  // You should have no business accessing these methods unless you're serializing
  const X509Certificates& certificates() const& { return mCertificates; }
  X509Certificates certificates()&& { return mCertificates; }
};

inline X509CertificateChain operator/(X509CertificateChain chain, X509Certificate leaf) { return chain /= std::move(leaf); }


class X509CertificateSigningRequest {
 public:
  X509CertificateSigningRequest() = default;
  /*!
   * \brief Construct a CSR for the provided key pair, common name and organizational unit.
   * \warning No check is performed on the provided data.
   */
  X509CertificateSigningRequest(AsymmetricKeyPair& keyPair, const std::string& commonName, const std::string& organizationalUnit);

  X509CertificateSigningRequest(const X509CertificateSigningRequest& other);
  X509CertificateSigningRequest& operator=(X509CertificateSigningRequest other);
  X509CertificateSigningRequest(X509CertificateSigningRequest&& other) noexcept;
  ~X509CertificateSigningRequest();

  AsymmetricKey getPublicKey() const;
  std::optional<std::string> getCommonName() const;
  std::optional<std::string> getOrganizationalUnit() const;
  [[nodiscard]] bool verifySignature() const;
  std::string toPem() const;
  std::string toDer() const;

  static X509CertificateSigningRequest FromPem(const std::string& pem);
  static X509CertificateSigningRequest FromDer(const std::string& der);

  /*!
   * \brief Generate a X509 certificate based on the CSR. As subject it will contain the common name returned by getCommonName() and the organizational unit returned by getOrganizationUnit(). Other fields will be ignored.
   * \param caCert The issuer's certificate
   * \param caPrivateKey Private key of the CA used to sign the certificate
   * \param validityPeriod Period in seconds that certificate should be valid
   * \return X509Certificate Newly generated certificate
   */
  X509Certificate signCertificate(const X509Certificate& caCert, const AsymmetricKey& caPrivateKey, const std::chrono::seconds validityPeriod) const;

 private:
  X509_REQ* mCSR = nullptr;

  std::optional<std::string> searchOIDinSubject(int nid) const;
};


class X509Identity {
private:
  AsymmetricKey mPrivateKey;
  X509CertificateChain mCertificateChain;

public:
  X509Identity(AsymmetricKey privateKey, X509CertificateChain certificateChain);

  static X509Identity MakeSelfSigned(std::string organization, std::string commonName, std::string countryCode = X509Certificate::defaultSelfSignedCountryCode, std::chrono::seconds validityPeriod = X509Certificate::defaultSelfSignedValidity);

  const AsymmetricKey& getPrivateKey() const noexcept { return mPrivateKey; }
  const X509CertificateChain& getCertificateChain() const noexcept { return mCertificateChain; }
};


class X509IdentityFilesConfiguration {
private:
  std::filesystem::path mPrivateKeyFilePath;
  std::filesystem::path mCertificateChainFilePath;
  X509Identity mIdentity;

public:
  X509IdentityFilesConfiguration(const Configuration& config, const std::string& keyPrefix);
  X509IdentityFilesConfiguration(std::filesystem::path privateKeyFilePath, std::filesystem::path certificateChainFilePath, std::filesystem::path rootCaCertFilePath);

  const std::filesystem::path& getPrivateKeyFilePath() const noexcept { return mPrivateKeyFilePath; }
  const std::filesystem::path& getCertificateChainFilePath() const noexcept { return mCertificateChainFilePath; }

  const X509Identity& identity() const noexcept { return mIdentity; }
};

}
