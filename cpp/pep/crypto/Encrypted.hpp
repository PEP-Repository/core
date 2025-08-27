#pragma once

#include <pep/serialization/Serialization.hpp>

namespace pep {

class EncryptedBase {
protected:
  std::string baseDecrypt(const std::string& key) const;

public:
  std::string mCiphertext;
  std::string mIv;
  std::string mTag;

  EncryptedBase() = default;
  EncryptedBase(
    std::string ciphertext,
    std::string iv,
    std::string tag) :
    mCiphertext(std::move(ciphertext)),
    mIv(std::move(iv)),
    mTag(std::move(tag)) { }
  EncryptedBase(
    const std::string& key,
    const std::string& plaintext);
};


template<typename T>
class Encrypted : public EncryptedBase {
public:
  Encrypted() = default; // TODO get rid of default constructor
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
