#pragma once

#include <list>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/serialization/Serializer.hpp>
#include <pep/utils/Configuration.hpp>

extern "C" {
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
}

namespace pep {

class X509Certificate {
 public:
  X509Certificate();
  explicit X509Certificate(const std::string& in);
  X509Certificate(const X509Certificate& o);
  X509Certificate& operator = (const X509Certificate& o);
  ~X509Certificate();
  AsymmetricKey getPublicKey() const;
  std::string getCommonName() const;
  std::string getOrganizationalUnit() const;
  std::string getIssuerCommonName() const;
  bool isServerCertificate() const;
  int64_t getValidityDuration() const;
  std::string toPem() const;
  std::string toDer() const;
  bool hasTLSServerEKU() const;

 private:
  mutable mbedtls_x509_crt mInternal;

  friend class X509Certificates;
  friend class X509CertificateChain;
  friend class X509CertificateSigningRequest;
  friend class Serializer<X509Certificate>; // TODO: remove
};

class X509Certificates: public std::list<X509Certificate> {
 public:
  using std::list<X509Certificate>::list;
  X509Certificates() = default;
  explicit X509Certificates(const std::string& in);

  std::string toPem() const;

 private:
  mbedtls_x509_crt* linkInternalCertificates() const;
  void unlinkInternalCertificates() const;

  friend class X509CertificateChain;
};

class X509RootCertificates: public X509Certificates {
 public:
  using X509Certificates::X509Certificates;
};

class X509CertificateChain : public X509Certificates {
 public:
  using X509Certificates::X509Certificates;

  bool verify(X509RootCertificates rootCAs, const char* commonName = nullptr) const;
  bool certifiesPrivateKey(const AsymmetricKey& privateKey) const;
};

class X509CertificateSigningRequest {
 public:
  X509CertificateSigningRequest();

  /*!
   * \brief Construct a CSR for the provided key pair, common name and organizational unit.
   * \warning No check is performed on the provided data.
   */
  X509CertificateSigningRequest(AsymmetricKeyPair& keyPair, const std::string& commonName, const std::string& organizationalUnit);

  explicit X509CertificateSigningRequest(const std::string& in);
  X509CertificateSigningRequest(const X509CertificateSigningRequest& o);
  X509CertificateSigningRequest& operator = (const X509CertificateSigningRequest& o);
  ~X509CertificateSigningRequest();
  AsymmetricKey getPublicKey() const;
  std::string getCommonName() const;
  std::string getOrganizationalUnit() const;
  bool isValidPublicKey() const;
  void verifySignature() const;

  /*!
   * \brief Generate a X509 certificate based on the CSR. As subject it will contain the common name returned by getCommonName() and the organizational unit returned by getOrganizationUnit(). Other fields will be ignored.
   *
   * \param caCert The issuer's certificate
   * \param caPrivateKey Private key of the CA used to sign the certificate
   * \param validityPeriod Period in seconds that certificate should be valid
   * \return pep::X509Certificate Newly generated certificate
   */
  X509Certificate signCertificate(
    const X509Certificate& caCert,
    const AsymmetricKey& caPrivateKey,
    uint32_t validityPeriod
  ) const;
  std::string toPem() const;
 protected:
  mutable mbedtls_x509_csr mInternal;

  friend class Serializer<X509CertificateSigningRequest>; // TODO: remove
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
