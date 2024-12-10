#pragma once

#include <pep/utils/Hasher.hpp>
#include <mbedtls/md5.h>

namespace pep {

// Md5 - needed for Amazon S3
class Md5 : public Hasher<std::string> {
private:
  mbedtls_md5_context mContext;

protected:
  void process(const void* block, size_t size) override;
  Hash finish() override;

public:
  Md5();
  ~Md5() noexcept override;
};

}
