#pragma once

#include <pep/crypto/Encrypted.hpp>
#include <pep/utils/Timestamp.hpp>

#include <string>

namespace pep {

struct SFId {
  std::string path;
  Timestamp time;
};

using EncryptedSFId = Encrypted<SFId>;

}
