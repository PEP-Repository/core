#pragma once

#include <pep/utils/Hasher.hpp>

#include <openssl/evp.h>

#include <string>

namespace pep {

class OpenSSLHasher : public Hasher<std::string> {
  EVP_MD_CTX* mCtx = nullptr;

  void process(const void* block, size_t size) override;
  Hash finish() override;

public:
  OpenSSLHasher(const EVP_MD* type);
  ~OpenSSLHasher() noexcept override;

  [[nodiscard]] std::size_t blockSize() const;
  [[nodiscard]] std::size_t digestSize() const;
};

// Md5 - needed for Amazon S3
struct Md5 : OpenSSLHasher { Md5(); };

struct Sha256 : OpenSSLHasher { Sha256(); };
struct Sha512 : OpenSSLHasher { Sha512(); };

}
