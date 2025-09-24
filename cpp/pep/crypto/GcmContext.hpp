#pragma once

#include <memory>
#include <openssl/evp.h>

namespace pep {

using GcmContext = std::unique_ptr<EVP_CIPHER_CTX, void(*)(EVP_CIPHER_CTX*)>;

inline GcmContext createGcmContext() {
  return GcmContext(EVP_CIPHER_CTX_new(),
      [](EVP_CIPHER_CTX* ctx) {
        EVP_CIPHER_CTX_free(ctx);
      });
}

}
