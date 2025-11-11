#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Random.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/storagefacility/DataPayloadPage.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/crypto/GcmContext.hpp>

#include <stdexcept>

namespace pep {

std::string DataPayloadPage::computeAdditionalData(
        const Metadata& metadata) const {
  auto scheme = metadata.getEncryptionScheme();
  if (scheme == EncryptionScheme::V1) {
    return Serialization::ToString(metadata, false);
  }
  if (scheme == EncryptionScheme::V2
      || scheme == EncryptionScheme::V3) {
    return PackUint64BE(mPageNumber);
  }
  throw std::runtime_error("Unknown page encryption scheme");
}

bool DataPayloadPage::EncryptionIncludesMetadata(EncryptionScheme encryptionScheme) {
  return encryptionScheme == EncryptionScheme::V1; // See the "computeAdditionalData" method
}

void DataPayloadPage::setEncrypted(
      std::string_view plaintext,
      const std::string& key,
      const Metadata& metadata) {
  int ret;
  int len;
  auto ctx = createGcmContext();
  RandomBytes(mCryptoNonce, 16);
  mCryptoMac.resize(16);
  mPayloadData.resize(plaintext.size());
  if (key.size() != 32)
    throw std::runtime_error("keys should be 32 bytes");
  std::string ad = computeAdditionalData(metadata);
  ret = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptInit_ex failed");
  ret = EVP_CIPHER_CTX_ctrl(
    ctx.get(),
    EVP_CTRL_GCM_SET_IVLEN,
    static_cast<int>(mCryptoNonce.size()),
    nullptr
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl IVLEN failed");
  ret = EVP_EncryptInit_ex(ctx.get(),
    nullptr,
    nullptr,
    reinterpret_cast<const uint8_t*>(key.data()),
    reinterpret_cast<const uint8_t*>(mCryptoNonce.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptInit_ex 2nd failed");
  ret = EVP_EncryptUpdate(ctx.get(),
    nullptr,
    &len,
    reinterpret_cast<const uint8_t*>(ad.data()),
    static_cast<int>(ad.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptUpdate for AD failed");
  ret = EVP_EncryptUpdate(ctx.get(),
    reinterpret_cast<uint8_t*>(mPayloadData.data()),
    &len,
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    static_cast<int>(plaintext.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptUpdate for plaintext failed");
  if (len != static_cast<int>(mPayloadData.size()))
    throw std::runtime_error("EVP_EncryptUpdate wrote wrong amount of data");
  ret = EVP_EncryptFinal_ex(
    ctx.get(),
    reinterpret_cast<uint8_t*>(mPayloadData.data() + len),
    &len
  );
  if (ret != 1)
    throw std::runtime_error("EVP_EncryptFinal failed");
  if (len != 0)
    throw std::runtime_error("EVP_EncryptFinal overshot");
  ret = EVP_CIPHER_CTX_ctrl(ctx.get(),
    EVP_CTRL_GCM_GET_TAG,
    static_cast<int>(mCryptoMac.size()),
    reinterpret_cast<int8_t*>(mCryptoMac.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl GET_TAG failed");
}

std::string DataPayloadPage::decrypt(
        const std::string& key, const Metadata& metadata) const {
  auto ctx = createGcmContext();
  int ret = 0;
  int len = 0;
  std::string plaintext(mPayloadData.size(), '\0');
  if (key.size() != 32)
    throw std::runtime_error("keys should be 32 bytes");
  std::string ad = computeAdditionalData(metadata);

  ret = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptInit_ex failed");
  ret = EVP_CIPHER_CTX_ctrl(
    ctx.get(),
    EVP_CTRL_GCM_SET_IVLEN,
    static_cast<int>(mCryptoNonce.size()),
    nullptr
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl IVLEN failed");
  ret = EVP_DecryptInit_ex(ctx.get(),
    nullptr,
    nullptr,
    reinterpret_cast<const uint8_t*>(key.data()),
    reinterpret_cast<const uint8_t*>(mCryptoNonce.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptInit_ex 2nd failed");
  ret = EVP_DecryptUpdate(ctx.get(),
    nullptr,
    &len,
    reinterpret_cast<const uint8_t*>(ad.data()),
    static_cast<int>(ad.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptUpdate for AD failed");
  ret = EVP_DecryptUpdate(ctx.get(),
    reinterpret_cast<uint8_t*>(plaintext.data()),
    &len,
    reinterpret_cast<const uint8_t*>(mPayloadData.data()),
    static_cast<int>(mPayloadData.size())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_DecryptUpdate for plaintext failed");
  if (len != static_cast<int>(plaintext.size()))
    throw std::runtime_error("EVP_DecryptUpdate wrote wrong amount of data");
  ret = EVP_CIPHER_CTX_ctrl(ctx.get(),
    EVP_CTRL_GCM_SET_TAG,
    static_cast<int>(mCryptoMac.size()),
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) Not written to for EVP_CTRL_GCM_SET_TAG
    const_cast<char *>(mCryptoMac.data())
  );
  if (ret != 1)
    throw std::runtime_error("EVP_CIPHER_CTX_ctrl TAG failed");
  ret = EVP_DecryptFinal_ex(
    ctx.get(),
    reinterpret_cast<uint8_t*>(plaintext.data() + len),
    &len
  );
  if (ret != 1)
    throw PageIntegrityError();
  if (len != 0)
    throw std::runtime_error("EVP_DecryptFinal overshot");
  return plaintext;
}

}
