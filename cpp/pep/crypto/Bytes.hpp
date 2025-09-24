#pragma once

#include <pep/crypto/Encrypted.hpp>

namespace pep {

// just a wrapper to conform to the serialization formalism
class Bytes {
public:
  Bytes(std::string data) : mData(std::move(data)) {}
  std::string mData;
};

using EncryptedBytes = Encrypted<Bytes>;

}
