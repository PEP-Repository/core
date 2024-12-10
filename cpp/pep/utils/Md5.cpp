#include <pep/utils/Md5.hpp>

#include <cassert>

namespace pep {

Md5::Md5() {
  mbedtls_md5_init(&mContext);
  [[maybe_unused]] auto error_code = mbedtls_md5_starts_ret(&mContext);
  assert(error_code == 0);
}

Md5::~Md5() noexcept {
  mbedtls_md5_free(&mContext);
}

void Md5::process(const void* block, size_t size) {
  [[maybe_unused]] auto error_code = mbedtls_md5_update_ret(&mContext,
    reinterpret_cast<const unsigned char*>(block),
    size);
  assert(error_code == 0);
}


Md5::Hash Md5::finish() {
  unsigned char digest[16];
  [[maybe_unused]] auto error_code= mbedtls_md5_finish_ret(&mContext, digest);
  assert(error_code == 0);
  return std::string(reinterpret_cast<char*>(digest), 16);
}

}
