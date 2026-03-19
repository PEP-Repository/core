
#include <pep/utils/OpenSSLHasher.hpp>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/OpensslUtils.hpp>

#include <cassert>

namespace pep {

OpenSSLHasher::OpenSSLHasher(const EVP_MD* type) {
  assert(type && "Cannot construct OpenSSLHasher with nullptr as hash algorithm");
  if (!(mCtx = EVP_MD_CTX_new())) {
    throw pep::OpenSSLError("Failed to create context in OpenSSLHasher constructor");
  }
  if (EVP_DigestInit_ex(mCtx, type, nullptr) <= 0) {
    throw pep::OpenSSLError("Failed to initialize context in OpenSSLHasher constructor");
  }
}

OpenSSLHasher::~OpenSSLHasher() noexcept {
  EVP_MD_CTX_free(mCtx);
}

std::size_t OpenSSLHasher::blockSize() const {
  auto size = EVP_MD_CTX_get_block_size(mCtx);
  if (size <= 0) {
    throw pep::OpenSSLError("Failed to get block size for OpenSSLHasher");
  }
  return static_cast<std::size_t>(size);
}

std::size_t OpenSSLHasher::digestSize() const {
  auto size = EVP_MD_CTX_get_size(mCtx);
  if (size <= 0) {
    throw pep::OpenSSLError("Failed to get digest size for OpenSSLHasher");
  }
  return static_cast<std::size_t>(size);
}

void OpenSSLHasher::process(const void* block, size_t size) {
  if (EVP_DigestUpdate(mCtx, block, size) <= 0) {
    throw pep::OpenSSLError("Failed to update digest in OpenSSLHasher::process");
  }
}

auto OpenSSLHasher::finish() -> Hash {
  std::string digest(digestSize(), '\0');
  unsigned written{};
  if (EVP_DigestFinal_ex(mCtx, ConvertBytes<unsigned char>(digest).data(), &written) <= 0) {
    throw pep::OpenSSLError("Failed to finalize digest in OpenSSLHasher::finish");
  }
  assert(written == digest.size());
  return digest;
}


Md5::Md5() : OpenSSLHasher(EVP_md5()) {}

Sha256::Sha256() : OpenSSLHasher(EVP_sha256()) {}
Sha512::Sha512() : OpenSSLHasher(EVP_sha512()) {}

}
