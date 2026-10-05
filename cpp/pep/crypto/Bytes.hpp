#pragma once

#include <pep/crypto/Encrypted.hpp>

namespace pep {

// just a wrapper to conform to the serialization formalism
struct Bytes {
  std::string data;
};

using EncryptedBytes = Encrypted<Bytes>;

}
