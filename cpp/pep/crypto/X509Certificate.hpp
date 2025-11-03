#pragma once

#include <openssl/x509.h>
#include <openssl/asn1.h>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/serialization/Serializer.hpp>
#include <pep/utils/Configuration_fwd.hpp>

#include <list>
#include <chrono>
#include <optional>
#include <string>
#include <filesystem>

namespace pep {

class X509Certificate {
 public:
  X509Certificate() = default;
  X509Certificate(const X509Certificate& other);
  X509Certificate& operator=(X509Certificate other);
  X509Certificate(X509Certificate&& other) noexcept;
  ~X509Certificate();
  AsymmetricKey getPublicKey() const;
  std::optional<std::string> getCommonName() const;
  std::optional<std::string> getOrganizationalUnit() const;
  std::optional<std::string> getIssuerCommonName() const;
  bool hasBasicConstraints() const;
  bool hasDigitalSignatureKeyUsage() const;
  std::optional<unsigned long> getPathLength() const; //NOLINT(google-runtime-int)
  [[nodiscard]] bool verifySubjectKeyIdentifier() const;
  [[nodiscard]] bool verifyAuthorityKeyIdentifier(const X509Certificate& issuerCert) const;
  bool isPEPServerCertificate() const;
  bool hasTLSServerEKU() const;
  std::chrono::sys_seconds getNotBefore() const;
  std::chrono::sys_seconds getNotAfter() const;
  bool isCurrentTimeInValidityPeriod() const;
  std::string toPem() const;
  std::string toDer() const;

  bool isInitialized() const { return mInternal != nullptr; }

  static X509Certificate FromPem(const std::string& pem);
  static X509Certificate FromDer(const std::string& der);

 private:
  X509* mInternal = nullptr;

  X509Certificate(X509* cert) : mInternal(cert) {}
  std::optional<std::string> searchOIDinSubject(int nid) const;

  friend class X509Certificates;
  friend class X509CertificateChain;
  friend class X509CertificateSigningRequest;
};

class X509Certificates: public std::list<X509Certificate> {
 public:
  using std::list<X509Certificate>::list;
  X509Certificates() = default;
  explicit X509Certificates(const std::string& in);

  std::string toPem() const;
  bool isCurrentTimeInValidityPeriod() const;

  friend class X509CertificateChain;
};

class X509RootCertificates: public X509Certificates {
 public:
  using X509Certificates::X509Certificates;
};

class X509CertificateChain : public X509Certificates {
 public:
  using X509Certificates::X509Certificates;

  [[nodiscard]] bool verify(const X509RootCertificates& rootCAs) const;
  bool certifiesPrivateKey(const AsymmetricKey& privateKey) const;
};

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

class X509IdentityFilesConfiguration {
private:
  std::filesystem::path mPrivateKeyFilePath;
  std::filesystem::path mCertificateChainFilePath;
  AsymmetricKey mPrivateKey;
  X509CertificateChain mCertificateChain;

public:
  X509IdentityFilesConfiguration(const Configuration& config, const std::string& keyPrefix);
  X509IdentityFilesConfiguration(const std::filesystem::path& privateKeyFilePath, const std::filesystem::path& certificateChainFilePath);

  const std::filesystem::path& getPrivateKeyFilePath() const noexcept { return mPrivateKeyFilePath; }
  const std::filesystem::path& getCertificateChainFilePath() const noexcept { return mCertificateChainFilePath; }
  const AsymmetricKey& getPrivateKey() const noexcept { return mPrivateKey; }
  const X509CertificateChain& getCertificateChain() const noexcept { return mCertificateChain; }
};

}
