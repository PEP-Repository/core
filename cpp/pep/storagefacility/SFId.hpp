#pragma once

#include <pep/crypto/Encrypted.hpp>
#include <cstdint>

namespace pep {

class SFId {
public:
  SFId(std::string path, uint64_t time) : mPath(std::move(path)), mTime(time) { }
  std::string mPath;
  uint64_t mTime = 0;
};

using EncryptedSFId = Encrypted<SFId>;

}
