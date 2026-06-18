#pragma once

#include <pep/crypto/Encrypted.hpp>

namespace pep {

// just a wrapper to conform to the serialization formalism
class Bytes {
public:
  Bytes(std::string data) : data_(std::move(data)) {}
  std::string data_;
};

using EncryptedBytes = Encrypted<Bytes>;

}
