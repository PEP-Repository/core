#include <pep/networking/TLSClient.hpp>
#include <pep/utils/Log.hpp>

#include <fstream>
#include <filesystem>

#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/algorithm/string/predicate.hpp>

#ifdef KEYLOG_FILE
#include <openssl/ssl.h>
#endif //KEYLOG_FILE

namespace pep {

static const std::string LOG_TAG ("NetioTLS");

#ifdef KEYLOG_FILE
std::ofstream keylog;

void keylog_callback(const SSL* ssl, const char* line) {
  keylog << line << std::endl;
}

void set_keylog_file(SSL_CTX* ctx) {
  if(keylog.is_open())
    return;

  keylog.open(KEYLOG_FILE, std::ios::app);
  if(!keylog.is_open()) {
    LOG(LOG_TAG, warning) << "Could not open SSLkeylogfile " << KEYLOG_FILE;
    return;
  }

  SSL_CTX_set_keylog_callback(ctx, keylog_callback);
}
#endif //KEYLOG_FILE

void TLSClientParameters::ensureContextInitialized(boost::asio::ssl::context& context) {
  if (!contextInitialized) {
    context.set_verify_mode(boost::asio::ssl::verify_peer);
    if (caCertFilepath.empty()) {
      context.set_default_verify_paths();
    }
    else {
      context.load_verify_file(std::filesystem::canonical(caCertFilepath).string());
    }
#ifdef KEYLOG_FILE
    set_keylog_file(context.native_handle());
#endif //KEYLOG_FILE
    //TODO Define TLS options
  }
}

bool TLSClientParameters::VerifyCertificateBasedOnExpectedCommonName(const std::string& expectedCommonName, bool preverified, boost::asio::ssl::verify_context& verifyCtx) {
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
    ASN1_OBJECT *oid = sk_ASN1_OBJECT_value(eku.get(), i);
    char txt[1024];

    if ((OBJ_obj2txt(txt, sizeof(txt), oid, 0) != -1)
      && strcmp(txt, "TLS Web Server Authentication") == 0) {
      foundWebServerEKU = true;
      break;
    }
  }
  if (!foundWebServerEKU) {
    LOG(LOG_TAG, warning) << "Certificate does not have the right EKU";
    return false;
  }

  // Check name on certificate
  X509_NAME* name = X509_get_subject_name(cert);
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

bool TLSClientParameters::IsSpecificSslError(const boost::system::error_code& error, int code) {
  return error.category() == boost::asio::error::ssl_category
      && ERR_GET_REASON(static_cast<decltype(ERR_get_error())>(error.value())) == code;
}

}
