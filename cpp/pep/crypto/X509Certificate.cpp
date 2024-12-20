#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/pem.h>
#include <mbedtls/md.h>

#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Platform.hpp>
#include <pep/utils/Random.hpp>
#include <pep/crypto/X509Certificate.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>

#include <ctime>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

const std::string LOG_TAG ("X509Certificate");

const std::string intermediateServerCaCommonName = "PEP Intermediate PEP Server CA";
const std::string intermediateServerTlsCaCommonName = "PEP Intermediate TLS CA";

/// Returns a 'YYYYMMDDhhmmss' string (no separators)
std::string FormatTime(const tm& time) {
  return (boost::format("%04d%02d%02d%02d%02d%02d")
      % (time.tm_year + 1900) % (time.tm_mon + 1) % time.tm_mday
      % time.tm_hour % time.tm_min % time.tm_sec).str();
}

std::string SearchOIDinSubject(const mbedtls_x509_name& subject, const std::string& oid) {
  const mbedtls_x509_name* name = &subject;

  // Iterate over all fields in name and stop once we found an item with the oid we're looking for
  while( name != nullptr ) {
    std::string_view subjectOid(reinterpret_cast<const char*>(name->oid.p), name->oid.len);
    if (oid == subjectOid) {
      return std::string(reinterpret_cast<const char*>(name->val.p), name->val.len);
    }

    name = name->next;
  }

  return "";
}

} // namespace

namespace pep {

X509Certificate::X509Certificate(const std::string& in) {
  size_t sz = in.size();
  mbedtls_x509_crt_init(&mInternal);

  // See AsymmetricKey::AsymmetricKey for why we do this
  if (in.starts_with("-----BEGIN ") && in.back() != '\0') {
    sz++;
  }

  if (const auto err = mbedtls_x509_crt_parse(&mInternal, reinterpret_cast<const unsigned char*>(in.c_str()), sz);
      err == 0) {
    if (mInternal.next != nullptr)
      mbedtls_x509_crt_free(mInternal.next);
    mInternal.next = nullptr;
  } else {
    throw std::runtime_error(std::string("Certificate parsing error ") + std::to_string(-err));
  }
}

X509Certificate::X509Certificate() {
  mbedtls_x509_crt_init(&mInternal);
}

X509Certificate::~X509Certificate() {
  mInternal.next = nullptr;
  mbedtls_x509_crt_free(&mInternal);
}

X509Certificate::X509Certificate(const X509Certificate& o) {
  if(&o != this) {
    mbedtls_x509_crt_init(&mInternal);
    mbedtls_x509_crt_parse_der(&mInternal, o.mInternal.raw.p, o.mInternal.raw.len);
  }
}

X509Certificate& X509Certificate::operator=(const X509Certificate& o) {
  if (&o != this) {
    mbedtls_x509_crt_free(&mInternal);
    mbedtls_x509_crt_init(&mInternal);
    mbedtls_x509_crt_parse_der(&mInternal, o.mInternal.raw.p, o.mInternal.raw.len);
  }

  return *this;
}

AsymmetricKey X509Certificate::getPublicKey() const {
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, mInternal.pk);
}

bool X509Certificate::hasTLSServerEKU() const {
  std::string_view serverAuthOid(MBEDTLS_OID_SERVER_AUTH);
  for (auto sequence = &mInternal.ext_key_usage; sequence != nullptr; sequence = sequence->next) {
    if (std::string_view(reinterpret_cast<const char*>(sequence->buf.p), sequence->buf.len) == serverAuthOid) {
      return true;
    }
  }

  return false;
}

std::string X509Certificate::getCommonName() const {
  return SearchOIDinSubject(mInternal.subject, MBEDTLS_OID_AT_CN);
}

std::string X509Certificate::getOrganizationalUnit() const {
  return SearchOIDinSubject(mInternal.subject, MBEDTLS_OID_AT_ORG_UNIT);
}

std::string X509Certificate::getIssuerCommonName() const {
  return SearchOIDinSubject(mInternal.issuer, MBEDTLS_OID_AT_CN);
}

bool X509Certificate::isServerCertificate() const {
  if (getCommonName() != getOrganizationalUnit()) {
    return false;
  }

  auto issuerCn = getIssuerCommonName();
  if (hasTLSServerEKU()) {
    return issuerCn == intermediateServerTlsCaCommonName;
  }

  return issuerCn == intermediateServerCaCommonName;
}

int64_t X509Certificate::getValidityDuration() const {
  mbedtls_x509_time v = mInternal.valid_to;
  struct tm expiry = {};
  expiry.tm_year = v.year - 1900;
  expiry.tm_mon = v.mon - 1;
  expiry.tm_mday = v.day;
  expiry.tm_hour = v.hour;
  expiry.tm_min = v.min;
  expiry.tm_sec  = v.sec;

  // Convert std::tm to time_point
  std::time_t expiry_time_t = timegm(&expiry); // Use timegm to get time in UTC
  auto expiry_time_point = std::chrono::system_clock::from_time_t(expiry_time_t);

  // Get the current time in UTC
  auto now = std::chrono::system_clock::now();

  // Calculate the difference in seconds
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(expiry_time_point - now).count();

  return static_cast<int64_t>(duration);
}

std::string X509Certificate::toPem() const {
  char buf[4096] = {0};
  size_t olen = 0;
  mbedtls_pem_write_buffer(
    "-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n",
    mInternal.raw.p, mInternal.raw.len,
    reinterpret_cast<unsigned char*>(buf), sizeof(buf), &olen);
  return std::string(buf);
}

std::string X509Certificate::toDer() const {
  return std::string(reinterpret_cast<char*>(mInternal.raw.p), mInternal.raw.len);
}

X509Certificates::X509Certificates(const std::string& in)
  : std::list<X509Certificate>() {
  size_t sz = in.size();
  mbedtls_x509_crt crt;
  mbedtls_x509_crt_init(&crt);

  // MbedTLS requires a null-terminated string, but only for PEM format, not for DER encoding
  if (in.starts_with("-----BEGIN ") && in.back() != '\0') {
    sz++;
  }

  int ret = mbedtls_x509_crt_parse(&crt, reinterpret_cast<const unsigned char*>(in.c_str()), sz);
  if (ret != 0) {
      char error_buf[256];
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      mbedtls_x509_crt_free(&crt);
    if (ret > 0) {
      throw std::runtime_error("Failed to parse " + std::to_string(ret) + " certificates in chain: " + std::string(error_buf));
    } else {
      throw std::runtime_error("Failed to parse certificate chain: " + std::string(error_buf));
    }
  }

  for (mbedtls_x509_crt* iter = &crt; iter != nullptr; iter = iter->next) {
    try {
        push_back(X509Certificate(std::string(reinterpret_cast<const char*>(iter->raw.p), iter->raw.len)));
    } catch (const std::exception& e) {
        mbedtls_x509_crt_free(&crt);
        throw;
    }
  }

  mbedtls_x509_crt_free(&crt);
}

std::string X509Certificates::toPem() const {
  std::string out;
  const_iterator it;

  for (it = begin(); it != end(); it++)
    out += it->toPem();

  return out;
}

mbedtls_x509_crt* X509Certificates::linkInternalCertificates() const {
  const_iterator prev, it;
  for(it = prev = begin(), it++; it != end(); prev = it, it++) {
    prev->mInternal.next = &it->mInternal;
  }

  return !empty() ? &begin()->mInternal : nullptr;
}

void X509Certificates::unlinkInternalCertificates() const {
  for(const_iterator it = begin(); it != end(); it++)
    it->mInternal.next = nullptr;
}

bool X509CertificateChain::certifiesPrivateKey(
    const AsymmetricKey& privateKey) const {
  if (begin() == end())
    return false;
  return privateKey.isPrivateKeyFor(begin()->getPublicKey());
}


using namespace std;

bool X509CertificateChain::verify(X509RootCertificates rootCAs, const char* commonName) const {
  if(begin() != end()) {
    uint32_t flags = 0;

    mbedtls_x509_crt* certificate_chain = linkInternalCertificates();
    // Could be nullptr (no CAs accepted)
    mbedtls_x509_crt* root_cas = rootCAs.linkInternalCertificates();

    const auto ret = mbedtls_x509_crt_verify(certificate_chain, root_cas, nullptr, commonName, &flags, nullptr, nullptr);
    if (ret != 0) {
      char buffer[128];
      LOG(LOG_TAG, error) << "root CAs=\n" << rootCAs.toPem() << "\n";
      LOG(LOG_TAG, error) << "certificate chain=\n" << toPem() << "\n";
      mbedtls_x509_crt_verify_info(buffer, sizeof(buffer), "mbedtls_x509_crt_verify : -", flags);
      LOG(LOG_TAG, error) << buffer;
    }
    unlinkInternalCertificates();
    rootCAs.unlinkInternalCertificates();

    return ret == 0;
  }

  return false;
}


X509CertificateSigningRequest::X509CertificateSigningRequest(const std::string& in) {
  size_t sz = in.size();
  mbedtls_x509_csr_init(&mInternal);

  if (in.starts_with("-----BEGIN ") && in.back() != '\0') {
    sz++;
  }

  mbedtls_x509_csr_parse(&mInternal, reinterpret_cast<const unsigned char*>(in.c_str()), sz);
}

X509CertificateSigningRequest::X509CertificateSigningRequest(AsymmetricKeyPair& keyPair, const std::string& commonName, const std::string& organizationalUnit) {
  mbedtls_x509_csr_init(&mInternal);

  mbedtls_x509write_csr writeCSR;
  mbedtls_x509write_csr_init(&writeCSR);
  mbedtls_x509write_csr_set_md_alg(&writeCSR, MBEDTLS_MD_SHA256);
  mbedtls_x509write_csr_set_key(&writeCSR, &keyPair.mCtx);

// Construct subject
//   std::stringstream subject;
//   subject << "OU=" << organizationalUnit << ",CN=" << commonName;
//   ret = mbedtls_x509write_csr_set_subject_name(&writeCSR, subject.str().c_str());
//   if(ret != 0) {
//     LOG(LOG_TAG, warning) << "Generation of CSR failed (mbedtls_x509write_csr_set_subject_name=" << ret << ")";
//     mbedtls_x509write_csr_free(&writeCSR);
//     throw std::runtime_error("Generation of CSR failed");
//   }
  /*
   * subject may in fact match any input since it is stored as TLV
   * this may be of future use, i.e. subject's pseudonym in the CN
   */
  //mbedtls_asn1_free_named_data_list(&writeCSR.subject);
  auto nd = mbedtls_asn1_store_named_data(
    &writeCSR.subject,
    MBEDTLS_OID_AT_CN,
    MBEDTLS_OID_SIZE(MBEDTLS_OID_AT_CN),
    reinterpret_cast<const unsigned char*>(commonName.data()),
    commonName.length()
  );
  if (nd == nullptr)
    throw std::runtime_error("mbedtls_asn1_store_named_data failed");
  nd->val.tag = MBEDTLS_ASN1_UTF8_STRING;
  nd = mbedtls_asn1_store_named_data(
    &writeCSR.subject,
    MBEDTLS_OID_AT_ORG_UNIT,
    MBEDTLS_OID_SIZE(MBEDTLS_OID_AT_ORG_UNIT),
    reinterpret_cast<const unsigned char*>(organizationalUnit.data()),
    organizationalUnit.length()
  );
  if (nd == nullptr)
    throw std::runtime_error("mbedtls_asn1_store_named_data failed");
  nd->val.tag = MBEDTLS_ASN1_UTF8_STRING;

  // Generate CSR. The CSR is written at the end of buf.
  uint8_t buf[4096];
  const int ret = mbedtls_x509write_csr_der(&writeCSR, buf, sizeof(buf), MbedRandomSource, nullptr);
  if (ret <= 0) {
    LOG(LOG_TAG, warning) << "Generation of CSR failed (mbedtls_x509write_csr_der=" << ret << ")";
    mbedtls_x509write_csr_free(&writeCSR);
    throw std::runtime_error("Generation of CSR failed");
  }

  // Import the generated CSR again
  auto ret2 = mbedtls_x509_csr_parse_der(&mInternal, buf + sizeof(buf) - ret, static_cast<size_t>(ret));  //TODO Is there any check here whether the CSR is valid? I.e. valid PK, correct signature, etc
  mbedtls_x509write_csr_free(&writeCSR);

  if(ret2 != 0) {
    std::string csr(
      reinterpret_cast<char*>(buf + sizeof(buf) - ret),
      static_cast<size_t>(ret));
    LOG(LOG_TAG, warning)
      << "Couldn't parse generated CSR (mbedtls_x509_csr_parse_der="
        << ret2 << ").  Generated: "
      << boost::algorithm::hex(csr);
    throw std::runtime_error("Generation of CSR failed");
  }
}

X509CertificateSigningRequest::X509CertificateSigningRequest() {
  mbedtls_x509_csr_init(&mInternal);
}

X509CertificateSigningRequest::~X509CertificateSigningRequest() {
  mbedtls_x509_csr_free(&mInternal);
}

X509CertificateSigningRequest::X509CertificateSigningRequest(const X509CertificateSigningRequest& o) {
  if(&o != this) {
    mbedtls_x509_csr_init(&mInternal);
    mbedtls_x509_csr_parse_der(&mInternal, o.mInternal.raw.p, o.mInternal.raw.len);
  }
}

X509CertificateSigningRequest& X509CertificateSigningRequest::operator=(const X509CertificateSigningRequest& o) {
  if (&o != this) {
    mbedtls_x509_csr_free(&mInternal);
    mbedtls_x509_csr_init(&mInternal);
    mbedtls_x509_csr_parse_der(&mInternal, o.mInternal.raw.p, o.mInternal.raw.len);
  }
  return *this;
}

std::string X509CertificateSigningRequest::getCommonName() const {
  return SearchOIDinSubject(mInternal.subject, MBEDTLS_OID_AT_CN);
}

std::string X509CertificateSigningRequest::getOrganizationalUnit() const {
  return SearchOIDinSubject(mInternal.subject, MBEDTLS_OID_AT_ORG_UNIT);
}

AsymmetricKey X509CertificateSigningRequest::getPublicKey() const {
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, mInternal.pk);
}

bool X509CertificateSigningRequest::isValidPublicKey() const {
  return (mbedtls_rsa_check_pubkey(mbedtls_pk_rsa(mInternal.pk)) == 0);
}

void X509CertificateSigningRequest::verifySignature() const {
  auto mdinfo = mbedtls_md_info_from_type(mInternal.sig_md);
  size_t md_len = mbedtls_md_get_size(mdinfo);
  std::vector<unsigned char> md(md_len);

  if (auto ret = mbedtls_md(mdinfo, mInternal.cri.p, mInternal.cri.len, md.data());
    ret != 0)
  {
    LOG(LOG_TAG, warning)
      << "Couldn't hash CSR for signature verification. Return code: " << ret;
    throw std::runtime_error("Couldn't hash CSR for signature verification");
  }

  if (auto ret = mbedtls_pk_verify_ext(mInternal.sig_pk, mInternal.sig_opts, &mInternal.pk, mInternal.sig_md, md.data(), md_len, mInternal.sig.p, mInternal.sig.len);
    ret != 0) {
    LOG(LOG_TAG, warning)
      << "Verification of CSR signature failed. Return code: " << ret;
    throw std::runtime_error("Verification of CSR signature failed");
  }
}

X509Certificate X509CertificateSigningRequest::signCertificate(
  const X509Certificate& caCert,
  const AsymmetricKey& caPrivateKey,
  uint32_t validityPeriod
) const {
  //TODO Check returned values

  mbedtls_x509write_cert mWrite;
  uint8_t buf[4096];

  // Add common name (CN) and organizational unit (OU) to certificate
  mbedtls_x509write_crt_init(&mWrite);
  mbedtls_asn1_free_named_data_list(&mWrite.subject);

  std::string commonName = getCommonName();
  std::string organizationalUnit = getOrganizationalUnit();
  auto nd = mbedtls_asn1_store_named_data(
    &mWrite.subject,
    MBEDTLS_OID_AT_CN,
    MBEDTLS_OID_SIZE(MBEDTLS_OID_AT_CN),
    reinterpret_cast<const unsigned char*>(commonName.data()),
    commonName.length()
  );
  if (nd == nullptr)
    throw std::runtime_error("mbedtls_asn1_store_named_data failed");
  nd->val.tag =  MBEDTLS_ASN1_UTF8_STRING;

  nd = mbedtls_asn1_store_named_data(
    &mWrite.subject,
    MBEDTLS_OID_AT_ORG_UNIT,
    MBEDTLS_OID_SIZE(MBEDTLS_OID_AT_ORG_UNIT),
    reinterpret_cast<const unsigned char*>(organizationalUnit.data()),
    organizationalUnit.length()
  );
  if (nd == nullptr)
    throw std::runtime_error("mbedtls_asn1_store_named_data failed");
  nd->val.tag =  MBEDTLS_ASN1_UTF8_STRING;

  // Generate a random serial number
  uint8_t sn[16];
  mbedtls_mpi sn_mpi = {
    /*.s = */1, // Apparently "0" produces negative serial numbers, whereas "1" produces nonnegative ones
    /*.n = */sizeof(sn) / sizeof(mbedtls_mpi_uint),
    /*.p = */reinterpret_cast<mbedtls_mpi_uint*>(sn)
  };
  RandomBytes(sn, 16);
  mbedtls_x509write_crt_set_serial(&mWrite, &sn_mpi);//TODO Check if return != 0

  // Set validity period (begin and end)
  auto unixSeconds = time(nullptr);
  struct tm tmNow, tmEnd;

  // We let certificates' validity start 60 seconds in the past
  // to account for clock drift.  See #677.
  unixSeconds -= 60;
  if (!gmtime_r(&unixSeconds, &tmNow)) {
    throw std::runtime_error("Failed to convert time");
  }
  unixSeconds += 60;
  unixSeconds += validityPeriod;
  if (!gmtime_r(&unixSeconds, &tmEnd)) {
    throw std::runtime_error("Failed to convert time");
  }

  auto strNow = FormatTime(tmNow),
       strEnd = FormatTime(tmEnd);
  LOG(LOG_TAG, debug) << "Cert start date: " << strNow <<std::endl;
  LOG(LOG_TAG, debug) << "Cert end date: " << strEnd <<std::endl;

  if(mbedtls_x509write_crt_set_validity(&mWrite, strNow.c_str(), strEnd.c_str()) != 0) {
    LOG(LOG_TAG, debug) << "mbedtls_x509write_crt_set_validity != 0" << std::endl;
  }//TODO Check if return != 0

  // Set digest algorithm and key
  mbedtls_x509write_crt_set_md_alg(&mWrite, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&mWrite, &mInternal.pk);

  // The generated certificate can only be used for digital signatures
  if(mbedtls_x509write_crt_set_key_usage(&mWrite, MBEDTLS_X509_KU_DIGITAL_SIGNATURE)!=0) {
    LOG(LOG_TAG, debug) << "mbedtls_x509write_crt_set_key_usage != 0" << std::endl;
  }//TODO Check if return != 0

  // Set CA private key used to sign certificate
  mbedtls_x509write_crt_set_issuer_key(&mWrite, &caPrivateKey.mCtx);

  /* copy cert subject to new cert's issuer
   * ref'ing does not work since linked list is ordered backwards
   * and verification of the resulting cert will not pass
   */
  mbedtls_asn1_free_named_data_list(&mWrite.issuer);
  const mbedtls_x509_name* name = &caCert.mInternal.subject;
  while( name != nullptr ) {
    nd = mbedtls_asn1_store_named_data(
      &mWrite.issuer,
      reinterpret_cast<const char*>(name->oid.p),
      name->oid.len,
      name->val.p,
      name->val.len
    );
    if (nd == nullptr)
      throw std::runtime_error("mbedtls_asn1_store_named_data failed");
    nd->val.tag = MBEDTLS_ASN1_UTF8_STRING;
    name = name->next;
  }

  if (mbedtls_x509write_crt_set_subject_key_identifier(&mWrite)!=0) {
    LOG(LOG_TAG, debug) << "mbedtls_x509write_crt_set_subject_key_identifier != 0" << std::endl;
  }//TODO Check if return != 0
  if (mbedtls_x509write_crt_set_authority_key_identifier(&mWrite)!=0) {
    LOG(LOG_TAG, debug) << "mbedtls_x509write_crt_set_authority_key_identifier != 0" << std::endl;
  }//TODO Check if return != 0

  X509Certificate output;

  if (const auto sz = mbedtls_x509write_crt_der(&mWrite, buf, sizeof(buf), MbedRandomSource, nullptr); sz > 0) {
    output = X509Certificate(std::string(reinterpret_cast<const char*>(buf + sizeof(buf) - sz), static_cast<size_t>(sz)));
    LOG(LOG_TAG, debug) << "Generation of certificate succeeded (size=" << sz << ")";
  } else {
    LOG(LOG_TAG, warning) << "Generation of certificate failed (err=" << sz << ")";
    mbedtls_x509write_crt_free(&mWrite);
    throw std::runtime_error("Generation of certificate failed");
  }

  mbedtls_x509write_crt_free(&mWrite);

  return output;
}

std::string X509CertificateSigningRequest::toPem() const {
  char buf[4096] = {0};
  size_t olen = 0;
  mbedtls_pem_write_buffer(
    "-----BEGIN CERTIFICATE REQUEST-----\n", "-----END CERTIFICATE REQUEST-----\n",
    mInternal.raw.p, mInternal.raw.len,
    reinterpret_cast<unsigned char*>(buf), sizeof(buf), &olen);//TODO Check if return != 0
  return std::string(buf);
}

X509IdentityFilesConfiguration::X509IdentityFilesConfiguration(const Configuration& config, const std::string& keyPrefix)
  : X509IdentityFilesConfiguration(config.get<std::filesystem::path>(keyPrefix + "PrivateKeyFile"), config.get<std::filesystem::path>(keyPrefix + "CertificateFile")) {
}

X509IdentityFilesConfiguration::X509IdentityFilesConfiguration(const std::filesystem::path& privateKeyFilePath, const std::filesystem::path& certificateChainFilePath)
  : mPrivateKeyFilePath(privateKeyFilePath),
  mCertificateChainFilePath(certificateChainFilePath),
  mPrivateKey(ReadFile(mPrivateKeyFilePath)),
  mCertificateChain(ReadFile(mCertificateChainFilePath)) {

  if (!mPrivateKey.isSet()) {
    throw std::runtime_error("privateKey must be set");
  }
  if (mCertificateChain.empty()) {
    throw std::runtime_error("certificateChain must not be empty");
  }
  if (!mCertificateChain.certifiesPrivateKey(mPrivateKey)) {
    throw std::runtime_error("certificateChain does not match private key");
  }
}

}
