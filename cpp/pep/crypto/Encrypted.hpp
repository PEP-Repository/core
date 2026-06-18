#pragma once

#include <pep/serialization/Serialization.hpp>

namespace pep {

class EncryptedBase {
protected:
  std::string baseDecrypt(const std::string& key) const;

public:
  std::string ciphertext_;
  std::string iv_;
  std::string tag_;

  EncryptedBase() = default;
  EncryptedBase(
    std::string ciphertext,
    std::string iv,
    std::string tag) :
    ciphertext_(std::move(ciphertext)),
    iv_(std::move(iv)),
    tag_(std::move(tag)) { }
  EncryptedBase(
    const std::string& key,
    const std::string& plaintext);
};


template<typename T>
class Encrypted : public EncryptedBase {
public:
  Encrypted(
    std::string ciphertext,
    std::string iv,
    std::string tag)
    : EncryptedBase(
      std::move(ciphertext),
      std::move(iv),
      std::move(tag)) { }
  Encrypted(const std::string& key, T o) :
    EncryptedBase(key, Serialization::ToString(std::move(o))) { }

  T decrypt(const std::string& key) const {
    return Serialization::FromString<T>(baseDecrypt(key));
  }
};

template <typename T>
struct NormalizedTypeNamer<Encrypted<T>> : public BasicNormalizedTypeNamer {
  static inline std::string GetTypeName() { return "Encrypted" + GetNormalizedTypeName<T>(); }
};

}
