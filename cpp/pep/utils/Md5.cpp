#include <openssl/evp.h>

#include <pep/utils/Md5.hpp>
#include <pep/utils/OpensslUtils.hpp>
#include <pep/utils/Defer.hpp>

namespace pep {

Md5::Md5() {
  mContext = EVP_MD_CTX_new();

  if (!mContext) {
    throw pep::OpenSSLError("Failed to create EVP_MD_CTX in Md5 constructor");
  }
  if (EVP_DigestInit_ex(mContext, EVP_md5(), nullptr) <= 0) {
    EVP_MD_CTX_free(mContext);
    throw pep::OpenSSLError("Failed to initialize MD5 context in Md5 constructor");
  }
}

Md5::~Md5() noexcept {
  EVP_MD_CTX_free(mContext);
}

void Md5::process(const void* block, size_t size) {
  if (EVP_DigestUpdate(mContext, reinterpret_cast<const unsigned char*>(block), size) <= 0) {
    throw pep::OpenSSLError("Failed to update MD5 context in Md5::process");
  }
}

Md5::Hash Md5::finish() {
  // MD5 digests are always 16 bytes (128 bit) long
  unsigned char digest[16];
  if (EVP_DigestFinal_ex(mContext, digest, nullptr) <= 0) {
    throw pep::OpenSSLError("Failed to finalize MD5 context in Md5::finish");
  }
  return std::string(reinterpret_cast<char*>(digest), 16);
}

}
