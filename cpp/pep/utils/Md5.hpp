#pragma once

#include <pep/utils/Hasher.hpp>
#include <openssl/evp.h>

namespace pep {

// Md5 - needed for Amazon S3
class Md5 : public Hasher<std::string> {
private:
  EVP_MD_CTX* mContext;

protected:
  void process(const void* block, size_t size) override;
  Hash finish() override;

public:
  Md5();
  ~Md5() noexcept override;
};

}
