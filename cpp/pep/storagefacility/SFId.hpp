#pragma once

#include <pep/crypto/Encrypted.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace pep {

struct SFId {
  std::string mPath;
  Timestamp mTime;
};

using EncryptedSFId = Encrypted<SFId>;

}
