#include <pep/networking/CertificateVerification.hpp>
#include <pep/utils/Log.hpp>
#include <boost/algorithm/string/predicate.hpp>

#ifdef _WIN32
#include <wincrypt.h>
#pragma comment(lib, "Crypt32")
#endif // _WIN32

namespace pep {

namespace {

const std::string LOG_TAG("Certificate verification");

}

void TrustSystemRootCAs(boost::asio::ssl::context& ctx) {
#ifndef _WIN32
  // No-op, since certificates-to-trust can be placed in the "Directory for OpenSSL files" on Unix-like platforms.
  // See https://www.madboa.com/geek/openssl/#what-certificate-authorities-does-openssl-recognize.
#else
  /*
  On Windows platforms using a binary OpenSSL distribution, the "Directory for OpenSSL files" is set to "/usr/ssl".
  (Check it on your Windows system by running "openssl version -d" from a command line).
  Since the directory is obviously unusable, we need a different way to find trusted root CA certificates.
  So we instruct OpenSSL to trust the "Trusted Root Certification Authorities" in the Windows Certificate Store.
  */

  // Code taken from https://stackoverflow.com/a/40710521
  HCERTSTORE hStore = ::CertOpenSystemStoreA(0, "ROOT");
  if (hStore == nullptr) {
    return;
  }

  X509_STORE* store = X509_STORE_new();
  PCCERT_CONTEXT pContext = nullptr;
  while ((pContext = ::CertEnumCertificatesInStore(hStore, pContext)) != nullptr) {
    // convert from DER to internal format
    const BYTE* certEncoded = pContext->pbCertEncoded;
    X509* x509 = d2i_X509(nullptr,
                          &certEncoded,
                          static_cast<long>(pContext->cbCertEncoded));
    if (x509 != nullptr) {
      X509_STORE_add_cert(store, x509);
      X509_free(x509);
    }
  }

  ::CertFreeCertificateContext(pContext);
  ::CertCloseStore(hStore, 0);

  // attach X509_STORE to boost ssl context
  SSL_CTX_set_cert_store(ctx.native_handle(), store);
#endif
}

bool VerifyCertificateBasedOnExpectedCommonName(const std::string& expectedCommonName, bool preverified, boost::asio::ssl::verify_context& verifyCtx) {
  LOG(LOG_TAG, debug) << "Checking certificate for expected commonName " << expectedCommonName;

  if (!preverified) {
    int err = X509_STORE_CTX_get_error(verifyCtx.native_handle());
    LOG(LOG_TAG, warning) << "Preverification of certificate failed with error " << err << " (" << X509_verify_cert_error_string(err) << ")";
    return false;
  }

  // Check if we are at the peer certificate at the end of the chain. If not, we do not verify this certificate
  assert(preverified);
  int depth = X509_STORE_CTX_get_error_depth(verifyCtx.native_handle());
  if (depth > 0) {
    return true;
  }

  X509* cert = X509_STORE_CTX_get_current_cert(verifyCtx.native_handle());

  // Check for the TLS Server extended key usage field.  See #674
  bool foundWebServerEKU = false;
  std::unique_ptr<EXTENDED_KEY_USAGE, void(*)(EXTENDED_KEY_USAGE*)> eku(
    static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(
      cert, NID_ext_key_usage, nullptr, nullptr)),
    [](EXTENDED_KEY_USAGE* eku) {
      if (eku != nullptr) sk_ASN1_OBJECT_pop_free(eku, ASN1_OBJECT_free);
    });
  if (eku == nullptr) {
    LOG(LOG_TAG, warning) << "Certificate does not contain EKU field";
    return false;
  }
  for (int i = 0; i < sk_ASN1_OBJECT_num(eku.get()); i++) {
    ASN1_OBJECT* oid = sk_ASN1_OBJECT_value(eku.get(), i);
    std::string txt(1024, '\0');
    if ((OBJ_obj2txt(txt.data(), static_cast<int>(txt.size()), oid, 0) != -1)
      && txt == "TLS Web Server Authentication") {
      foundWebServerEKU = true;
      break;
    }
  }
  if (!foundWebServerEKU) {
    LOG(LOG_TAG, warning) << "Certificate does not have the right EKU";
    return false;
  }

  // Check name on certificate
  auto name = X509_get_subject_name(cert); // Don't declare explicitly as X509_NAME* because wincrypt.h defines a macro of that name
  int i = -1;
  ASN1_STRING* asn1CommonName = nullptr;
  while ((i = X509_NAME_get_index_by_NID(name, NID_commonName, i)) >= 0) {
    X509_NAME_ENTRY* name_entry = X509_NAME_get_entry(name, i);
    asn1CommonName = X509_NAME_ENTRY_get_data(name_entry);
  }

  if (asn1CommonName && asn1CommonName->data && asn1CommonName->length) {
    //const char* commonName = reinterpret_cast<const char*>(asn1CommonName->data);
    std::string commonName(
      reinterpret_cast<const char*>(asn1CommonName->data),
      static_cast<size_t>(asn1CommonName->length));
    LOG(LOG_TAG, debug) << "Received certificate with commonName " << commonName;
    if (expectedCommonName == commonName) {
      LOG(LOG_TAG, debug) << "Expected commonName (" << expectedCommonName << ") matched with received commonName (" << commonName << ")";
      return true;
    }
    else if (commonName[0] == '*' && commonName[1] == '.' && boost::algorithm::ends_with(expectedCommonName, commonName.substr(1))) {
      LOG(LOG_TAG, debug) << "Expected commonName (" << expectedCommonName << ") matched with received wildcard commonName (" << commonName << ")";
      return true;
    }
  }

  LOG(LOG_TAG, warning) << "Certificate verification failed";
  return false;
}

}
