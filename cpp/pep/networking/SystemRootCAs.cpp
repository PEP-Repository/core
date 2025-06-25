#include <pep/networking/SystemRootCAs.hpp>

#ifdef _WIN32
#include <wincrypt.h>
#pragma comment(lib, "Crypt32")
#endif // _WIN32

namespace pep {

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

}
