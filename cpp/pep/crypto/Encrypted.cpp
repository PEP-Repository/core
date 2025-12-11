#include <pep/crypto/Encrypted.hpp>
#include <pep/crypto/GcmContext.hpp>
#include <pep/utils/Random.hpp>

namespace pep {

EncryptedBase::EncryptedBase(const std::string& key,
  const std::string& plaintext) {
  auto ctx = createGcmContext();
  RandomBytes(mIv, 16);
  mTag.resize(16);
  mCiphertext.resize(plaintext.size());
  if (key.size() != 32)
    throw std::runtime_error("keys should be 32 bytes");
  int ret{};
  ret = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptInit_ex failed");
  ret = EVP_CIPHER_CTX_ctrl(
    ctx.get(),
    EVP_CTRL_GCM_SET_IVLEN,
    static_cast<int>(mIv.size()),
    nullptr
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl IVLEN failed");
  ret = EVP_EncryptInit_ex(ctx.get(),
    nullptr,
    nullptr,
    reinterpret_cast<const uint8_t*>(key.data()),
    reinterpret_cast<const uint8_t*>(mIv.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptInit_ex 2nd failed");
  int len{};
  ret = EVP_EncryptUpdate(ctx.get(),
    reinterpret_cast<uint8_t*>(mCiphertext.data()),
    &len,
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    static_cast<int>(plaintext.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptUpdate for plaintext failed");
  if (len != static_cast<int>(mCiphertext.size()))
    throw std::runtime_error("EVP_EncryptUpdate wrote wrong amount of data");
  ret = EVP_EncryptFinal_ex(
    ctx.get(),
    reinterpret_cast<uint8_t*>(mCiphertext.data() + len),
    &len
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptFinal failed");
  if (len != 0)
    throw std::runtime_error("EVP_EncryptFinal overshot");
  ret = EVP_CIPHER_CTX_ctrl(ctx.get(),
    EVP_CTRL_GCM_GET_TAG,
    static_cast<int>(mTag.size()),
    reinterpret_cast<int8_t*>(mTag.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl GET_TAG failed");
}

std::string EncryptedBase::baseDecrypt(const std::string& key) const {
  auto ctx = createGcmContext();
  int ret = 0;
  int len = 0;
  std::string plaintext;
  plaintext.resize(mCiphertext.size());
  if (key.size() != 32)
    throw std::runtime_error("keys should be 32 bytes");

  ret = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptInit_ex failed");
  ret = EVP_CIPHER_CTX_ctrl(
    ctx.get(),
    EVP_CTRL_GCM_SET_IVLEN,
    static_cast<int>(mIv.size()),
    nullptr
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl IVLEN failed");
  ret = EVP_DecryptInit_ex(ctx.get(),
    nullptr,
    nullptr,
    reinterpret_cast<const uint8_t*>(key.data()),
    reinterpret_cast<const uint8_t*>(mIv.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptInit_ex 2nd failed");
  ret = EVP_DecryptUpdate(ctx.get(),
    reinterpret_cast<uint8_t*>(plaintext.data()),
    &len,
    reinterpret_cast<const uint8_t*>(mCiphertext.data()),
    static_cast<int>(mCiphertext.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptUpdate for plaintext failed");
  if (len != static_cast<int>(plaintext.size()))
    throw std::runtime_error("EVP_DecryptUpdate wrote wrong amount of data");
  ret = EVP_CIPHER_CTX_ctrl(ctx.get(),
    EVP_CTRL_GCM_SET_TAG,
    static_cast<int>(mTag.size()),
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) Not written to for EVP_CTRL_GCM_SET_TAG
    const_cast<char*>(mTag.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl TAG failed");
  ret = EVP_DecryptFinal_ex(
    ctx.get(),
    reinterpret_cast<uint8_t*>(plaintext.data() + len),
    &len
  );
  if (ret != 1)
    throw std::runtime_error("Cryptographic integrity error");
  if (len != 0)
    throw std::runtime_error("EVP_DecryptFinal overshot");
  return plaintext;
}

}
