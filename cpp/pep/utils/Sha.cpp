#include <pep/utils/Sha.hpp>

#include <pep/utils/OpensslUtils.hpp>

#include <cassert>

namespace pep {

EVP_MD_CTX* detail::ShaBase::ensureInitialized() {
  if (mdctx == nullptr) {
    mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
      throw OpenSSLError("Failed to create EVP_MD_CTX in SHA constructor");
    }
    if (EVP_DigestInit_ex(mdctx, this->getDigestType(), nullptr) <= 0) {
      throw OpenSSLError("Failed to initialize EVP_MD_CTX in SHA constructor");
    }
  }
  return mdctx;
}

void detail::ShaBase::digestInto(std::span<std::uint8_t> out) {
  unsigned int s{};
  if (EVP_DigestFinal_ex(this->ensureInitialized(), out.data(), &s) <= 0) {
    throw pep::OpenSSLError("Failed to finalize SHA digest");
  }
  assert(s == out.size());
}

void detail::ShaBase::process(const void* block, size_t size) {
  if (EVP_DigestUpdate(this->ensureInitialized(), block, size) <= 0) {
    throw OpenSSLError("Failed to update SHA digest");
  }
}

auto detail::ShaBase::finishImpl(size_t outputSize) -> Hash {
  std::string ret(outputSize, '\0');
  digestInto(ConvertBytes<std::uint8_t>(ret));
  return ret;
}

detail::ShaBase::~ShaBase() noexcept {
  EVP_MD_CTX_free(mdctx);
}


const EVP_MD* Sha256::getDigestType() const { return EVP_sha256(); }
const EVP_MD* Sha512::getDigestType() const { return EVP_sha512(); }

}
