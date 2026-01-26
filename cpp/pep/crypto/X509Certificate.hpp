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

class X509Extension {
public:
  X509Extension(const X509Extension& other);
  X509Extension(X509Extension&& other) noexcept;
  ~X509Extension() noexcept;
  X509Extension& operator=(X509Extension other);
  X509Extension& operator=(X509Extension&& other) noexcept;

  explicit X509Extension(X509_EXTENSION* extension) noexcept : mRaw(extension) {}

  bool isCritical() const noexcept;
  std::string getName() const;
  std::string getValue() const;
private:
  X509_EXTENSION* mRaw;
};

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

  bool hasSameSubject(const X509Certificate& other) const;

  static X509Certificate FromPem(const std::string& pem);
  static X509Certificate FromDer(const std::string& der);
  static X509Certificate MakeSelfSigned(const AsymmetricKeyPair& keys, std::string_view organization, std::string_view commonName, std::string_view countryCode = defaultSelfSignedCountryCode, std::chrono::seconds validityPeriod = defaultSelfSignedValidity);

  std::strong_ordering operator<=>(const X509Certificate& other) const;
  // We have a non-defaulted operator<=>. Unfortunately, only a defaulted operator<=> comes with a synthesized operator==.
  // So we have to add it explicitly.
  // Furthermore, a defaulted operator== would bypass our operator<=>, so that would just compare the mRaw pointers.
  // We therefore implement it ourselves so we can explicitly call our custom operator<=>.
  bool operator==(const X509Certificate& other) const { return operator<=>(other) == std::strong_ordering::equal; };

 private:
  X509* mRaw = nullptr;

  [[nodiscard]] X509& raw() const noexcept; // Some of our "const" methods require the X509 structure to be mutable
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

  const X509Certificate& leaf() const;
  X509CertificateChain& operator/=(X509Certificate leaf);

  [[nodiscard]] bool verify(const X509RootCertificates& rootCAs) const;
  bool isCurrentTimeInValidityPeriod() const;
  bool certifiesPrivateKey(const AsymmetricKey& privateKey) const;

  // You should have no business accessing these methods unless you're serializing
  const X509Certificates& certificates() const& { return mCertificates; }
  X509Certificates certificates()&& { return mCertificates; }

  auto operator<=>(const X509CertificateChain&) const = default;
};

inline X509CertificateChain operator/(X509CertificateChain chain, X509Certificate leaf) { return chain /= std::move(leaf); }


class X509CertificateSigningRequest {
 public:
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
  [[nodiscard]] std::vector<X509Extension> getExtensions() const;
  [[nodiscard]] bool verifySignature() const;
  std::string toPem() const;
  std::string toDer() const;

  static X509CertificateSigningRequest FromPem(const std::string& pem);
  static X509CertificateSigningRequest FromDer(const std::string& der);

  static X509CertificateSigningRequest CreateWithSubjectFromExistingCertificate(AsymmetricKeyPair& keyPair, const X509Certificate& certificate);

  /*!
   * \brief Generate a X509 certificate based on the CSR. As subject it will contain the common name returned by getCommonName() and the organizational unit returned by getOrganizationUnit(). Other fields will be ignored.
   * \param caCert The issuer's certificate
   * \param caPrivateKey Private key of the CA used to sign the certificate
   * \param validityPeriod Period in seconds that certificate should be valid
   * \return X509Certificate Newly generated certificate
   */
  X509Certificate signCertificate(const X509Certificate& caCert, const AsymmetricKey& caPrivateKey, const std::chrono::seconds validityPeriod) const;

 private:
  // This will create a certificate signing request that does not have a subject yet, and therefore can't yet be signed.
  // Make sure to set a subject and sign the certificate, before using it or returning it from a public method/constructor.
  static X509CertificateSigningRequest MakeStub(AsymmetricKeyPair& keyPair);

  void sign(const AsymmetricKeyPair& keyPair);

  X509_REQ* mCSR = nullptr;

  explicit X509CertificateSigningRequest(X509_REQ& csr) noexcept : mCSR(&csr) {} // Takes ownership
  std::optional<std::string> searchOIDinSubject(int nid) const;
};


class X509Identity {
private:
  AsymmetricKey mPrivateKey;
  X509CertificateChain mCertificateChain;

public:
  X509Identity(AsymmetricKey privateKey, X509CertificateChain certificateChain);

  static X509Identity MakeSelfSigned(std::string_view organization, std::string_view commonName, std::string_view countryCode = X509Certificate::defaultSelfSignedCountryCode, std::chrono::seconds validityPeriod = X509Certificate::defaultSelfSignedValidity);

  const AsymmetricKey& getPrivateKey() const noexcept { return mPrivateKey; }
  const X509CertificateChain& getCertificateChain() const noexcept { return mCertificateChain; }
};


class X509IdentityFiles {
private:
  std::filesystem::path mPrivateKeyFilePath;
  std::filesystem::path mCertificateChainFilePath;
  std::shared_ptr<X509Identity> mIdentity;

public:
  X509IdentityFiles(std::filesystem::path privateKeyFilePath, std::filesystem::path certificateChainFilePath, std::filesystem::path rootCaCertFilePath);

  static X509IdentityFiles FromConfig(const Configuration& config, const std::string& keyPrefix);

  const std::filesystem::path& getPrivateKeyFilePath() const noexcept { return mPrivateKeyFilePath; }
  const std::filesystem::path& getCertificateChainFilePath() const noexcept { return mCertificateChainFilePath; }
  std::shared_ptr<const X509Identity> identity() const noexcept { return mIdentity; }
  std::shared_ptr<X509Identity> identity() noexcept { return mIdentity; }
};

}
