#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include <boost/numeric/conversion/cast.hpp>

#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Platform.hpp> //for windows timegm
#include <pep/utils/OpensslUtils.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Shared.hpp>

#include <pep/crypto/X509Certificate.hpp>

#include <ctime> //for tm, time_t
#include <chrono>
#include <string>
#include <optional>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <filesystem>
#include <array>
#include <span>
#include <string_view>

namespace pep {

static const std::string LOG_TAG ("X509Certificate");

// Max duration of certificate issued is 2 years, change this to std::chrono::years{2} when this is fully supported
constexpr std::chrono::seconds MAX_PEP_CERTIFICATE_VALIDITY_PERIOD = std::chrono::hours{17520};

namespace {

std::optional<std::string> SearchOIDinName(X509_NAME* name, int nid) {

  int index = X509_NAME_get_index_by_NID(name, nid, -1);
  if (index == -1) {
    LOG(LOG_TAG, error) << "Failed to obtain index for NID: " << nid << " in SearchOIDinName.";
    return std::nullopt;
  } else if (index == -2) {
    throw pep::OpenSSLError("Invalid NID: " + std::to_string(nid) + " in SearchOIDinName.");
  }

  X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, index);
  if (!entry) {
    throw pep::OpenSSLError("Failed to obtain entry object for NID: " + std::to_string(nid) + " in SearchOIDinName.");
  }

  ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
  if (!data) {
    throw pep::OpenSSLError("Failed to obtain entry data for NID: " + std::to_string(nid) + " in SearchOIDinName.");
  }

  // Convert ASN1_STRING to UTF-8, ASN1_STRING_to_UTF8 allocates memory for the UTF-8 string
  unsigned char* utf8{};
  int length = ASN1_STRING_to_UTF8(&utf8, data);

  // Free the UTF-8 string allocated by ASN1_STRING_to_UTF8
  PEP_DEFER(OPENSSL_free(utf8));

  if (length < 0) {
    throw pep::OpenSSLError("Failed to convert entry data to utf8 for NID: " + std::to_string(nid) + " in SearchOIDinName.");
  }

  std::string result(reinterpret_cast<char*>(utf8), static_cast<std::size_t>(length));

  return result;
}

// Warning: We are assuming that the KI extension is a SHA-1 hash of the public key,
// which is not by RFC definition always the case. Openssl may change this behavior in the future, breaking our test.
// In that case re-evaluate the testing of the certificate extensions or fix this function.
bool VerifyKeyIdentifier(const ASN1_OCTET_STRING* ki, const X509& cert) {
  // If KI extension is missing
  if (!ki) {
    throw std::invalid_argument("Key Identifier extension is missing in VerifyKeyIdentifier.");
  }

  // Retrieve the KI data and length from ASN1 byte string
  const unsigned char* kiData = ASN1_STRING_get0_data(ki);
  int kiLength = ASN1_STRING_length(ki);

  // Compute SHA-1 hash of the public key
  std::array<unsigned char, SHA_DIGEST_LENGTH> hash{};
  unsigned int hashLength = 0;
  if (X509_pubkey_digest(&cert, EVP_sha1(), hash.data(), &hashLength) <= 0) {
    throw pep::OpenSSLError("Failed to compute public key digest in VerifyKeyIdentifier.");
  }

  // Convert the KI data and hash to std::string_view
  std::string_view kiView = SpanToString(std::span(kiData, static_cast<size_t>(kiLength)));
  std::string_view hashView = SpanToString(std::span(hash.data(), static_cast<size_t>(hashLength)));

  // Compare the computed hash with the KI from the certificate
  return kiView == hashView;
}

std::chrono::sys_seconds ConvertASN1TimeToTimePoint(const ASN1_TIME* asn1Time) {
  // Convert ASN1_TIME to struct tm
  std::tm timeTm{};
  if (ASN1_TIME_to_tm(asn1Time, &timeTm) <= 0) {
    throw pep::OpenSSLError("Failed to convert ASN1_TIME to tm struct in X509Certificate.");
  }

  // Convert std::tm to time_point
  std::time_t time_in_time_t = ::timegm(&timeTm); // Use timegm to get time in UTC
  return time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(time_in_time_t));
}

bool HasExtensionFlag(X509& x509, uint32_t flag) {
  auto flags = X509_get_extension_flags(&x509);
  return (flags & flag) != 0;
}

} // namespace

X509Certificate::~X509Certificate() noexcept {
  X509_free(mRaw); // https://docs.openssl.org/master/man3/X509_new/#description: "If the argument is NULL, nothing is done."
}

X509Certificate::X509Certificate(const X509Certificate& other) {
  *this = other;
}

X509Certificate& X509Certificate::operator=(const X509Certificate& other) {
  if (this != &other) {
    X509* clone = X509_dup(&other.raw());
    if (!clone) {
      throw pep::OpenSSLError("Failed to duplicate X509 certificate in X509Certificate assignment operator.");
    }
    PEP_DEFER(X509_free(clone));

    // Rebuilds cached data: https://www.openssl.org/docs/manmaster/man3/X509_dup.html
    int result = X509_check_purpose(clone, -1, 0);
    if (result != 1) {
      throw pep::OpenSSLError("Failed to rebuild cached data by X509_check_purpose in X509Certificate assignment operator.");
    }

    // Save the new state and let the PEP_DEFERred code invoke X509_free on the state that we used to have (if any).
    // https://docs.openssl.org/master/man3/X509_new/#description about X509_free: "If the argument is NULL, nothing is done."
    std::swap(mRaw, clone);
  }
  return *this;
}

X509Certificate& X509Certificate::operator=(X509Certificate&& other) noexcept {
  std::swap(this->mRaw, other.mRaw);
  return *this;
}

X509Certificate::X509Certificate(X509Certificate&& other) noexcept {
  *this = std::move(other);
}

AsymmetricKey X509Certificate::getPublicKey() const {
  EVP_PKEY* pkey = X509_get0_pubkey(&this->raw());
  if (!pkey) {
    throw pep::OpenSSLError("Failed to get public key from X509 certificate in X509Certificate::getPublicKey.");
  }
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, pkey);
}

bool X509Certificate::hasBasicConstraints() const {
  return HasExtensionFlag(*this->rawPointer(), EXFLAG_BCONS);
}

/**
 * @brief Check whether the certificate has the digital signature key usage.
 * @note The key usage extension must be present in the certificate. This method
 *      returns false for unrestricted keys.
 */
bool X509Certificate::hasDigitalSignatureKeyUsage() const {
  auto internal = this->rawPointer();

  // Check if the KU extension is present
  if (!HasExtensionFlag(*internal, EXFLAG_KUSAGE)) {
    return false;
  }

  // Check if the digital signature key KU is set
  uint32_t keyUsage = X509_get_key_usage(internal);
  return (keyUsage & KU_DIGITAL_SIGNATURE) != 0;
}

/**
 * @brief Check whether the certificate has the TLS server extended key usage (EKU).
 * @note The extended key usage extension must be present in the certificate. This method
 *      returns false for unrestricted keys.
 */
bool X509Certificate::hasTLSServerEKU() const {
  auto internal = this->rawPointer();

  // Check if the EKU extension is present
  if (!HasExtensionFlag(*internal, EXFLAG_XKUSAGE)) {
    return false;
  }

  // Check if the TLS server EKU is set
  uint32_t extKeyUsage = X509_get_extended_key_usage(internal);
  return (extKeyUsage & XKU_SSL_SERVER) != 0;
}

bool X509Certificate::isSelfSigned() const {
  auto result = X509_self_signed(mRaw, 1);

  switch (result) { // https://docs.openssl.org/3.3/man3/X509_verify/#return-values
  case 1: // "if the signature is valid"
    return true;
  case 0: // "if the signature check fails"
    return false;
  case -1: // "If the signature could not be checked at all"
    throw OpenSSLError("Could not determine if certificate was self signed");
  default:
    throw OpenSSLError("Undocumented return value " + std::to_string(result) + " from X509_self_signed");
  }
}

/**
 * @brief Check the path lengths constraint of the X509Certificate.
 * @return The path length constraint, or std::nullopt if not present.
 */
//NOLINTBEGIN(google-runtime-int)
std::optional<unsigned long> X509Certificate::pathLengthConstraint() const {
  long pathLength = X509_get_pathlen(this->rawPointer());
  if (pathLength == -1) {
    return std::nullopt;
  }
  return static_cast<unsigned long>(pathLength);
}
//NOLINTEND(google-runtime-int)

bool X509Certificate::verifySubjectKeyIdentifier() const {
  auto internal = this->rawPointer();

  // Retrieve the Subject Key Identifier (SKI)
  const ASN1_OCTET_STRING* ski = X509_get0_subject_key_id(internal);
  return VerifyKeyIdentifier(ski, *internal);
}

bool X509Certificate::verifyAuthorityKeyIdentifier(const X509Certificate& issuerCert) const {
  // Retrieve the Authority Key Identifier (AKI) from this certificate
  const ASN1_OCTET_STRING* aki = X509_get0_authority_key_id(this->rawPointer());
  return VerifyKeyIdentifier(aki, issuerCert.raw());
}

const X509& X509Certificate::raw() const noexcept {
  return *this->rawPointer();
}

[[nodiscard]] X509* X509Certificate::rawPointer() const noexcept {
  assert(mRaw != nullptr);
  return mRaw;
}

std::optional<std::string> X509Certificate::searchOIDinSubject(int nid) const {
  X509_NAME* subjectName = X509_get_subject_name(this->rawPointer());
  return SearchOIDinName(subjectName, nid);
}

std::optional<std::string> X509Certificate::getCommonName() const {
  return searchOIDinSubject(NID_commonName);
}

std::optional<std::string> X509Certificate::getOrganizationalUnit() const {
  return searchOIDinSubject(NID_organizationalUnitName);
}

std::optional<std::string> X509Certificate::getIssuerCommonName() const {
  X509_NAME* issuerName = X509_get_issuer_name(&this->raw());
  return SearchOIDinName(issuerName, NID_commonName);
}

std::chrono::sys_seconds X509Certificate::getNotBefore() const {
  const ASN1_TIME* notBefore = X509_get0_notBefore(&this->raw());
  return ConvertASN1TimeToTimePoint(notBefore);
}

std::chrono::sys_seconds X509Certificate::getNotAfter() const {
  const ASN1_TIME* notAfter = X509_get0_notAfter(&this->raw());
  return ConvertASN1TimeToTimePoint(notAfter);
}

bool X509Certificate::isCurrentTimeInValidityPeriod() const {
  auto now = std::chrono::system_clock::now();
  auto notBefore = getNotBefore();
  auto notAfter = getNotAfter();

  return (now >= notBefore && now <= notAfter);
}

std::string X509Certificate::toPem() const {
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) {
    throw pep::OpenSSLError("Failed to create IO buffer (BIO) in X509Certificate::toPem.");
  }
  PEP_DEFER(BIO_free(bio));

  if (PEM_write_bio_X509(bio, &this->raw()) <= 0) {
    throw pep::OpenSSLError("Failed to write certificate to IO buffer (BIO) in X509Certificate::toPem.");
  }

  return OpenSSLBIOToString(bio);
}

std::string X509Certificate::toDer() const {
  // Create a BIO for the DER-encoded data
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    throw pep::OpenSSLError("Failed to create BIO in X509Certificate::toDer.");
  }
  PEP_DEFER(BIO_free(bio));

  // Convert the X509 structure to DER format and write it to the BIO
  if (i2d_X509_bio(bio, &this->raw()) <= 0) {
    throw pep::OpenSSLError("Failed to convert X509 to DER in X509Certificate::toDer.");
  }

  return OpenSSLBIOToString(bio);
}

X509Certificate X509Certificate::FromPem(const std::string& pem) {

  // Create a BIO for the PEM-encoded data
  BIO* bio = BIO_new_mem_buf(pem.data(), boost::numeric_cast<int>(pem.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create BIO in X509Certificate::FromPem.");
  }
  PEP_DEFER(BIO_free(bio));

  // Read the X509 structure from the BIO
  X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  if (!cert) {
    throw pep::OpenSSLError("Failed to parse PEM-encoded certificate in X509Certificate::FromPem.");
  }

  return X509Certificate(*cert);
}

X509Certificate X509Certificate::FromDer(const std::string& der) {

  // Create a BIO for the DER-encoded data
  BIO* bio = BIO_new_mem_buf(der.data(), boost::numeric_cast<int>(der.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create BIO in X509Certificate::FromDer.");
  }
  PEP_DEFER(BIO_free(bio));

  // Read the X509 structure from the BIO
  X509* cert = d2i_X509_bio(bio, nullptr);
  if (!cert) {
    throw pep::OpenSSLError("Failed to parse DER-encoded certificate in X509Certificate::FromDer.");
  }

  return X509Certificate(*cert);
}

X509Certificate X509Certificate::MakeSelfSigned(const AsymmetricKeyPair& keys, std::string organization, std::string commonName, std::string countryCode, std::chrono::seconds validityPeriod) {
  X509_NAME* name = X509_NAME_new();
  if (name == nullptr) {
    throw pep::OpenSSLError("Failed to create X509_NAME for self-signed certificate");
  }
  PEP_DEFER(X509_NAME_free(name));

  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<unsigned char*>(countryCode.data()), -1, -1, 0); // "country code"
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<unsigned char*>(organization.data()), -1, -1, 0); // "organization ('O')"
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<unsigned char*>(commonName.data()), -1, -1, 0); // "common name ('CN')"

  auto pub = keys.getPublicKey();
  assert(pub.mKey != nullptr);
  auto result = MakeUnsigned(pub, *name, validityPeriod);

  auto priv = keys.getPrivateKey();
  assert(priv.mKey != nullptr);
  result.sign(priv, *name);

  return result;
}

bool X509Certificate::operator==(const X509Certificate& rhs) const {
  if (this == &rhs) { // Reference equality
    return true;
  }
  return this->toPem() == rhs.toPem(); // TODO: make more efficient
}

X509CertificateChain::X509CertificateChain(X509Certificates certificates)
  : mCertificates(std::move(certificates)) {
  if (mCertificates.empty()) {
    throw std::runtime_error("Certificate chain cannot be empty"); // We need (to be able to produce) a leaf certificate
  }
}

/**
 * @brief Factory method for X509Certificates that takes a PEM-encoded list of certificates.
 * @param in The PEM-encoded certificate chain.
 */
X509Certificates FromPem(const std::string& in) {
  if (in.empty()) {
    throw std::runtime_error("Certificates input is empty in X509Certificates constructor.");
  }

  BIO *bio = BIO_new_mem_buf(in.data(), boost::numeric_cast<int>(in.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create IO buffer (BIO) in X509Certificates constructor.");
  }
  PEP_DEFER(BIO_free(bio));

  X509Certificates result;
  while (true) {
    X509* cert = nullptr;
    cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    if (!cert) {
      // Break out of the loop if cert is nullptr, indicating either end of file or an error
      break;
    }
    result.emplace_back(X509Certificate(*cert));
  }

  // When the while loop ends, it's usually just EOF, otherwise throw the error
  auto err = ERR_peek_last_error();
  if (ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    ERR_clear_error();
  } else {
    throw pep::OpenSSLError("Failed to parse certificate chain from IO buffer (BIO) in X509Certificates constructor.");
  }

  return result;
}

/**
 * @brief Convert the X509Certificates to PEM format.
 * @return The PEM-encoded certificate chain.
 */
std::string ToPem(const X509Certificates& certificates) {
  std::string out;

  for (const X509Certificate& cert : certificates) {
    out += cert.toPem();
  }

  return out;
}

X509RootCertificates::X509RootCertificates(X509Certificates certificates)
  : mItems(std::move(certificates)) {
  for (const auto& cert : mItems) {
    if (!cert.isSelfSigned()) {
      throw std::runtime_error("Root CA certificate is not self signed");
    }
    if (!cert.isCurrentTimeInValidityPeriod()) {
      throw std::runtime_error("Root CA certificate has expired");
    }
    /* TODO: perform (the) proper validation that this is a root CA
    if (!cert.hasDigitalSignatureKeyUsage()) { // Untrue for certificates generated using X509Certificate::MakeSelfSigned
      throw std::runtime_error("Root CA certificate cannot be used for digital signatures");
    }
    */
  }
}

X509RootCertificates X509RootCertificates::FromFile(const std::filesystem::path& caCertFilePath) {
  return X509RootCertificates(FromPem(ReadFile(caCertFilePath)));
}

bool X509CertificateChain::isCurrentTimeInValidityPeriod() const {
  return std::all_of(mCertificates.begin(), mCertificates.end(), std::mem_fn(&X509Certificate::isCurrentTimeInValidityPeriod));
}

bool X509CertificateChain::certifiesPrivateKey(const AsymmetricKey& privateKey) const {
  // Check whether the given private key corresponds to the public key of the leaf certificate
  return privateKey.isPrivateKeyFor(this->leaf().getPublicKey());
}

const X509Certificate& X509CertificateChain::leaf() const {
  assert(!mCertificates.empty());
  return mCertificates.front();
}

X509CertificateChain& X509CertificateChain::operator/=(X509Certificate leaf) {
  mCertificates.push_front(std::move(leaf));
  return *this;
}

bool X509CertificateChain::verify(const X509RootCertificates& rootCAs) const { // TODO: move code to constructor; validate order
  // https://stackoverflow.com/questions/16291809/programmatically-verify-certificate-chain-using-openssl-api
  // https://stackoverflow.com/questions/3412032/how-do-you-verify-a-public-key-was-issued-by-your-private-ca

  // Create a stack for the trusted root certificates
  STACK_OF(X509)* trusted = sk_X509_new_reserve(nullptr, static_cast<int>(mCertificates.size()));
  if (!trusted) {
    throw pep::OpenSSLError("Failed to create trusted STACK_OF(X509) root certificates in X509CertificateChain::verify.");
  }
  PEP_DEFER(sk_X509_free(trusted));

  // Add root certificates to the trusted stack
  for (const X509Certificate& rootCert : rootCAs.items()) {
    if (sk_X509_push(trusted, rootCert.rawPointer()) <= 0) {
      throw pep::OpenSSLError("Failed to push root certificate to trusted STACK_OF(X509) in X509CertificateChain::verify.");
    }
  }

  // Create a stack for the untrusted certificates
  STACK_OF(X509)* untrusted = sk_X509_new_reserve(nullptr, static_cast<int>(mCertificates.size()));
  if (!untrusted) {
    throw pep::OpenSSLError("Failed to create untrusted STACK_OF(X509) in X509CertificateChain::verify.");
  }
  PEP_DEFER(sk_X509_free(untrusted));

  // Add the certificates to the untrusted stack
  for (const X509Certificate& cert : mCertificates) {
    if (sk_X509_push(untrusted, cert.rawPointer()) <= 0) {
      throw pep::OpenSSLError("Failed to push certificate to untrusted STACK_OF(X509) in X509CertificateChain::verify.");
    }
  }

  // Create a new X509_STORE_CTX for the verification
  X509_STORE_CTX* ctx = X509_STORE_CTX_new();
  if (!ctx) {
    throw pep::OpenSSLError("Failed to create X509_STORE_CTX in X509CertificateChain::verify.");
  }
  PEP_DEFER(X509_STORE_CTX_free(ctx));

  // Initialize the X509_STORE_CTX with the certificate chain
  if (X509_STORE_CTX_init(ctx, nullptr, leaf().rawPointer(), untrusted) <= 0) {
    throw pep::OpenSSLError("Failed to initialize X509_STORE_CTX in X509CertificateChain::verify.");
  }

  // Add the tusted root certificates to the X509_STORE_CTX
  X509_STORE_CTX_set0_trusted_stack(ctx, trusted);

  X509_VERIFY_PARAM* param = X509_VERIFY_PARAM_new();
  if (!param) {
    throw pep::OpenSSLError("Failed to initialize X509_VERIFY_PARAM in X509CertificateChain::verify.");
  }

  if (X509_VERIFY_PARAM_set_purpose(param, X509_PURPOSE_ANY) <= 0) {
    throw pep::OpenSSLError("Failed to set purpose in X509CertificateChain::verify.");
  }

  // Depth of 1 means that the chain can have at most one intermediate CA certificate
  X509_VERIFY_PARAM_set_depth(param, 1);

  // Set security level, for the levels see: https://docs.openssl.org/master/man3/SSL_CTX_set_security_level/#default-callback-behaviour
  X509_VERIFY_PARAM_set_auth_level(param, 2);

  X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_X509_STRICT);

  X509_STORE_CTX_set0_param(ctx, param);

  int result = X509_verify_cert(ctx);
  if (result < 0) {
    throw pep::OpenSSLError("Failure during certificate chain verification in X509CertificateChain::verify.");
  } else if (result == 0) {
    PEP_DEFER(ERR_clear_error());
    std::string diagnostic = X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx));
    LOG(LOG_TAG, error) << "Verification failed with error string: " << diagnostic << " in X509CertificateChain::verify.";
    LOG(LOG_TAG, error) << "Leaf certificate public key: " << leaf().getPublicKey().toPem();
    return false;
  }

  return true;
}

/**
 * @brief Constructor for X509CertificateSigningRequest that generates a new CSR.
 * @param keyPair The key pair to use for the CSR.
 * @param commonName The common name for the CSR.
 * @param organizationalUnit The organizational unit for the CSR.
 */
X509CertificateSigningRequest::X509CertificateSigningRequest(AsymmetricKeyPair& keyPair, const std::string& commonName, const std::string& organizationalUnit) {

  if (!keyPair.mKeyPair) {
    throw std::invalid_argument("Input key pair is nullptr in X509CertificateSigningRequest constructor.");
  }

  // Create a new X509_REQ object
  mCSR = X509_REQ_new();
  if (!mCSR) {
    throw pep::OpenSSLError("Failed to create X509_REQ object in X509CertificateSigningRequest constructor.");
  }

  if (X509_REQ_set_version(mCSR, X509_REQ_VERSION_1) <= 0) {
    X509_REQ_free(mCSR);
    throw pep::OpenSSLError("Failed to set version in X509CertificateSigningRequest constructor.");
  }

  // Obtain the subject name of the X509 request, pointer must not be freed
  X509_NAME* name = X509_REQ_get_subject_name(mCSR);

  // Add the common name and organizational unit to the subject (name, OID, type, value, length, position, set)
  // position of -1 means it is appended, and set of 0 means a new RDN is created
  if(X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(commonName.data()), static_cast<int>(commonName.size()), -1, 0) <= 0) {
    X509_REQ_free(mCSR);
    throw pep::OpenSSLError("Failed to add CN in X509CertificateSigningRequest constructor.");
  }

  if(X509_NAME_add_entry_by_NID(name, NID_organizationalUnitName, MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(organizationalUnit.data()), static_cast<int>(organizationalUnit.size()), -1, 0) <= 0) {
    X509_REQ_free(mCSR);
    throw pep::OpenSSLError("Failed to add OU in X509CertificateSigningRequest constructor.");
  }

  // Set the public key
  if(X509_REQ_set_pubkey(mCSR, keyPair.mKeyPair) <= 0) {
    X509_REQ_free(mCSR);
    throw pep::OpenSSLError("Failed to set public key in X509CertificateSigningRequest constructor.");
  }

  // Sign the X509 request
  if(X509_REQ_sign(mCSR, keyPair.mKeyPair, EVP_sha256()) <= 0) {
    X509_REQ_free(mCSR);
    throw pep::OpenSSLError("Failed to sign X509 request in X509CertificateSigningRequest constructor.");
  }
}

X509CertificateSigningRequest::X509CertificateSigningRequest(const X509CertificateSigningRequest& other) {
  if (other.mCSR) {
    mCSR = X509_REQ_dup(other.mCSR);
    if (!mCSR) {
      throw pep::OpenSSLError("Failed to duplicate X509_REQ object in X509CertificateSigningRequest copy constructor.");
    }
  } else {
    mCSR = nullptr;
  }
}

X509CertificateSigningRequest& X509CertificateSigningRequest::operator=(X509CertificateSigningRequest other) {
  std::swap(mCSR, other.mCSR);
  return *this;
}

X509CertificateSigningRequest::X509CertificateSigningRequest(X509CertificateSigningRequest&& other) noexcept : mCSR(other.mCSR) {
  other.mCSR = nullptr;
}

X509CertificateSigningRequest::~X509CertificateSigningRequest() {
  X509_REQ_free(mCSR);
}

std::optional<std::string> X509CertificateSigningRequest::searchOIDinSubject(int nid) const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509 structure");
  }

  X509_NAME* subjectName = X509_REQ_get_subject_name(mCSR);
  return SearchOIDinName(subjectName, nid);
}

std::optional<std::string> X509CertificateSigningRequest::getCommonName() const {
  return searchOIDinSubject(NID_commonName);
}

std::optional<std::string> X509CertificateSigningRequest::getOrganizationalUnit() const {
  return searchOIDinSubject(NID_organizationalUnitName);
}

AsymmetricKey X509CertificateSigningRequest::getPublicKey() const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509_REQ structure");
  }
  EVP_PKEY* pkey = X509_REQ_get0_pubkey(mCSR);
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, pkey);
}

bool X509CertificateSigningRequest::verifySignature() const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509_REQ structure");
  }

  EVP_PKEY *pubKey = X509_REQ_get0_pubkey(mCSR);
  if (!pubKey) {
    throw pep::OpenSSLError("Couldn't get public key from CSR.");
  }

  int result = X509_REQ_verify(mCSR, pubKey);

  if (result == 1) {
    return true;
  } else if (result == 0) {
    auto errors = pep::TakeOpenSSLErrors();
    LOG(LOG_TAG, error) << "Failed to verify CSR signature." << errors;
    return false;
  } else {
    throw pep::OpenSSLError("Error while verifying CSR signature.");
  }
}

std::string X509CertificateSigningRequest::toPem() const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509_REQ structure");
  }

  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) {
    throw pep::OpenSSLError("Failed to create IO buffer (BIO) in X509CertificateSigningRequest::toPem.");
  }
  PEP_DEFER(BIO_free(bio));

  if (PEM_write_bio_X509_REQ(bio, mCSR) <= 0) {
    throw pep::OpenSSLError("Failed to write certificate request to IO buffer (BIO) in X509CertificateSigningRequest::toPem.");
  }

  return OpenSSLBIOToString(bio);
}

std::string X509CertificateSigningRequest::toDer() const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509_REQ structure");
  }

  // Create a BIO for the DER-encoded data
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    throw pep::OpenSSLError("Failed to create BIO in X509CertificateSigningRequest::toDer.");
  }
  PEP_DEFER(BIO_free(bio));

  // Convert the X509 structure to DER format and write it to the BIO
  if (i2d_X509_REQ_bio(bio, mCSR) <= 0) {
    throw pep::OpenSSLError("Failed to convert X509_REQ to DER in X509CertificateSigningRequest::toDer.");
  }

  return OpenSSLBIOToString(bio);
}

X509CertificateSigningRequest X509CertificateSigningRequest::FromPem(const std::string& pem) {

  BIO *bio = BIO_new_mem_buf(pem.data(), boost::numeric_cast<int>(pem.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create IO buffer (BIO) in X509CertificateSigningRequest::FromPem.");
  }
  PEP_DEFER(BIO_free(bio));

  X509_REQ* csr = PEM_read_bio_X509_REQ(bio, nullptr, nullptr, nullptr);
  if (!csr) {
    throw pep::OpenSSLError("Failed to parse CSR in X509CertificateSigningRequest::FromPem.");
  }

  // Create an X509CertificateSigningRequest object and set its internal CSR
  X509CertificateSigningRequest request;
  request.mCSR = csr;

  return request;
}

X509CertificateSigningRequest X509CertificateSigningRequest::FromDer(const std::string& der) {

  // Create a BIO for the DER-encoded data
  BIO* bio = BIO_new_mem_buf(der.data(), boost::numeric_cast<int>(der.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create BIO in X509CertificateSigningRequest::FromDer.");
  }
  PEP_DEFER(BIO_free(bio));

  // Read the X509_REQ structure from the BIO
  X509_REQ* csr = d2i_X509_REQ_bio(bio, nullptr);
  if (!csr) {
    throw pep::OpenSSLError("Failed to parse DER-encoded CSR in X509CertificateSigningRequest::FromDer.");
  }

  // Create an X509CertificateSigningRequest object and set its internal CSR
  X509CertificateSigningRequest request;
  request.mCSR = csr;

  return request;
}

X509Certificate X509Certificate::MakeUnsigned(const AsymmetricKey& publicKey, const X509_NAME& subjectName, const std::chrono::seconds validityPeriod) {
  // Sanity check on the validity period
  if (validityPeriod <= std::chrono::seconds{0}) {
    throw std::invalid_argument("Validity period must be greater than zero");
  }
  if (validityPeriod > MAX_PEP_CERTIFICATE_VALIDITY_PERIOD) {
    throw std::invalid_argument("Validity period exceeds the maximum allowed duration");
  }

  // Initialize the certificate writing context using the X509Certificate constructor
  X509* cert = X509_new();
  if (!cert) {
    throw pep::OpenSSLError("Failed to create X509 certificate in X509CertificateSigningRequest::signCertificate.");
  }
  // Take ownership of the X509 structure immediately to prevent it from being leaked if an exception is raised below
  X509Certificate result(*cert);

  // Set version to 2 (which corresponds to X509v3)
  if (X509_set_version(cert, X509_VERSION_3) <= 0) {
    throw pep::OpenSSLError("Failed to set certificate version");
  }

  // Generate a random serial number for the certificate using BIGNUM
  BIGNUM* bn = BN_new();
  if (!bn) {
    throw pep::OpenSSLError("Failed to allocate BIGNUM for serial number.");
  }
  PEP_DEFER(BN_free(bn));

  // Generate a random number of 128 bits, the top bit is set to 1 with BN_RAND_TOP_ONE, so that
  // the number is nonzero and always has the same length
  if (BN_rand(bn, 128, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) <= 0) {
    throw pep::OpenSSLError("Failed to generate random serial number for certificate.");
  }

  // Convert the BIGNUM to an ASN1_INTEGER
  ASN1_INTEGER* serialNumber = ASN1_INTEGER_new();
  if (!serialNumber) {
    throw pep::OpenSSLError("Failed to allocate ASN1_INTEGER for serial number.");
  }
  PEP_DEFER(ASN1_INTEGER_free(serialNumber));

  if (!BN_to_ASN1_INTEGER(bn, serialNumber)) {
    throw pep::OpenSSLError("Failed to convert BIGNUM to ASN1_INTEGER.");
  }

  // Set the serial number
  if (X509_set_serialNumber(cert, serialNumber) <= 0) {
    throw pep::OpenSSLError("Failed to set serial number for certificate.");
  }

  // Set the validity period, start 60 seconds before the current time
  if (!X509_gmtime_adj(X509_getm_notBefore(cert), -60)) {
    throw pep::OpenSSLError("Failed to set notBefore time in X509CertificateSigningRequest::signCertificate.");
  }
  //NOLINTNEXTLINE(google-runtime-int)
  if (!X509_gmtime_adj(X509_getm_notAfter(cert), boost::numeric_cast<long>(validityPeriod.count()))) {
    throw pep::OpenSSLError("Failed to set notAfter time in X509CertificateSigningRequest::signCertificate.");
  }

  // Set subject name
  if (X509_set_subject_name(cert, &subjectName) <= 0) {
    throw pep::OpenSSLError("Failed to set subject name.");
  }

  // Set public key
  if (X509_set_pubkey(cert, publicKey.mKey) <= 0) {
    throw pep::OpenSSLError("Failed to set public key.");
  }

  // The function `X509V3_EXT_conf_nid` is not documented, but we are using it as we lack a good alternative.
  // When a better documented high level function to create x509 extensions is found, this function should be replaced.
  // This comment can be deleted when openssl documentation is updated.
  // The function uses the openssl value syntax: https://docs.openssl.org/master/man5/x509v3_config/#description
  // Add Key Usage extension
  {
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_key_usage, "critical,digitalSignature");
    if (!ext) {
      throw pep::OpenSSLError("Failed to create Key Usage extension.");
    }
    PEP_DEFER(X509_EXTENSION_free(ext));

    if (X509_add_ext(cert, ext, -1) <= 0) {
      throw pep::OpenSSLError("Failed to add Key Usage extension to certificate.");
    }
  }

  return result;
}

void X509Certificate::sign(const AsymmetricKey& caPrivateKey, const X509_NAME& caName) {
  auto internal = this->rawPointer();

  // Set issuer name from CA's subject
  if (X509_set_issuer_name(internal, &caName) <= 0) {
    throw pep::OpenSSLError("Failed to set issuer name.");
  }

  // Sign the certificate with CA's private key, X509_sign returns the size
  // of the signature in bytes for success and zero for failure.
  if (X509_sign(internal, caPrivateKey.mKey, EVP_sha256()) <= 0) {
    throw pep::OpenSSLError("Failed to sign the certificate.");
  }
}

/**
 * @brief Sign a certificate based on the X509CertificateSigningRequest.
 * @param caCert The issuer's certificate.
 * @param caPrivateKey The private key of the CA used to sign the certificate.
 * @param validityPeriod seconds that the certificate should be valid.
 * @return The newly generated certificate.
 */
X509Certificate X509CertificateSigningRequest::signCertificate(const X509Certificate& caCert, const AsymmetricKey& caPrivateKey, const std::chrono::seconds validityPeriod) const {
  if (!mCSR) {
    throw std::runtime_error("Invalid X509_REQ structure");
  }
  if (!caPrivateKey.isSet()) {
    throw std::invalid_argument("CA private key is not set in X509CertificateSigningRequest::signCertificate.");
  }

  auto rawCaCert = caCert.rawPointer();

  auto result = X509Certificate::MakeUnsigned(this->getPublicKey(), *X509_REQ_get_subject_name(mCSR), validityPeriod);
  auto cert = result.rawPointer();

  X509V3_CTX ctx;
  X509V3_set_ctx(&ctx, rawCaCert, cert, mCSR, nullptr, 0);

  // Add Subject Key Identifier extension
  {
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_key_identifier, "hash");
    if (!ext) {
      throw pep::OpenSSLError("Failed to create Subject Key Identifier extension.");
    }
    PEP_DEFER(X509_EXTENSION_free(ext));

    if (X509_add_ext(cert, ext, -1) <= 0) {
      throw pep::OpenSSLError("Failed to add Subject Key Identifier extension to certificate.");
    }
  }

  // Add Authority Key Identifier extension
  {
    X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_authority_key_identifier, "keyid:always,issuer");
    if (!ext) {
      throw pep::OpenSSLError("Failed to create Authority Key Identifier extension.");
    }
    PEP_DEFER(X509_EXTENSION_free(ext));

    if (X509_add_ext(cert, ext, -1) <= 0) {
      throw pep::OpenSSLError("Failed to add Authority Key Identifier extension to certificate.");
    }
  }

  result.sign(caPrivateKey, *X509_get_subject_name(rawCaCert));

  return result;
}

X509Identity::X509Identity(AsymmetricKey privateKey, X509CertificateChain certificateChain)
  : mPrivateKey(std::move(privateKey)), mCertificateChain(std::move(certificateChain)) {
  if (!mCertificateChain.certifiesPrivateKey(mPrivateKey)) {
    throw std::runtime_error("certificateChain does not match private key");
  }
}

X509Identity X509Identity::MakeSelfSigned(std::string organization, std::string commonName, std::string countryCode) {
  auto keys = AsymmetricKeyPair::GenerateKeyPair();
  auto cert = X509Certificate::MakeSelfSigned(keys, std::move(organization), std::move(commonName), std::move(countryCode));
  return X509Identity(std::move(keys).getPrivateKey(), X509CertificateChain({ std::move(cert) }));
}

X509IdentityFilesConfiguration::X509IdentityFilesConfiguration(const Configuration& config, const std::string& keyPrefix) : X509IdentityFilesConfiguration(
  config.get<std::filesystem::path>(keyPrefix + "PrivateKeyFile"),
  config.get<std::filesystem::path>(keyPrefix + "CertificateFile"),
  config.get<std::filesystem::path>("CACertificateFile")) {
}

X509IdentityFilesConfiguration::X509IdentityFilesConfiguration(std::filesystem::path privateKeyFilePath, std::filesystem::path certificateChainFilePath, std::filesystem::path rootCaCertFilePath)
  : mPrivateKeyFilePath(std::move(privateKeyFilePath)),
  mCertificateChainFilePath(std::move(certificateChainFilePath)),
  mIdentity(AsymmetricKey(ReadFile(mPrivateKeyFilePath)),
    X509CertificateChain(FromPem(ReadFile(mCertificateChainFilePath)))) {
  LOG(LOG_TAG, debug) << "Added X509IdentityFiles from Configuration";
  if (!mIdentity.getCertificateChain().verify(X509RootCertificates::FromFile(rootCaCertFilePath))) {
    throw std::runtime_error("X509 identity does not pass validation against root CAs");
  }
}

}
