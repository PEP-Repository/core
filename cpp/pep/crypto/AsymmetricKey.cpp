#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/pem.h>
#include <mbedtls/error.h>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Bitpacking.hpp>

#include <optional>
#include <sstream>

namespace pep {

static const std::string LOG_TAG ("AsymmetricKey");

AsymmetricKey::AsymmetricKey() {
  mbedtls_pk_init(&mCtx);
}

AsymmetricKey::~AsymmetricKey() {
  mbedtls_pk_free(&mCtx);
}

AsymmetricKey::AsymmetricKey(asymmetric_key_type_t keyType, mbedtls_pk_context& o)
  : AsymmetricKey() {
  this->keyType = keyType;
  int res;
  uint8_t buf[4096];

  switch (keyType) {
  case ASYMMETRIC_KEY_TYPE_PRIVATE:
    res = mbedtls_pk_write_key_der(&o, buf, 4096);
    if (res >= 0) {
      res = mbedtls_pk_parse_key(
        &mCtx,
        buf + 4096 - res,
        static_cast<size_t>(res),
        nullptr,
      0);
    }
    if (res == 0) {
      set = true;
    }
    break;
  case ASYMMETRIC_KEY_TYPE_PUBLIC:
    res = mbedtls_pk_write_pubkey_der(&o, buf, 4096);
    if (res >= 0) {
      res = mbedtls_pk_parse_public_key(
        &mCtx,
        buf + 4096 - res,
        static_cast<size_t>(res)
      );
    }
    if (res == 0) {
      set = true;
    }
    break;
  case ASYMMETRIC_KEY_TYPE_NONE:
    throw std::runtime_error("Asymmetric key type not set");
  }
}

AsymmetricKey::AsymmetricKey(const AsymmetricKey& o)
  : AsymmetricKey() {
  *this = o;
}

AsymmetricKey::AsymmetricKey(const std::string& buf) : AsymmetricKey() {
  auto dwSize = buf.size();

  /* For some reason mbed wants a PEM-encoded something to always end with a nullbyte
   * even though we pass its length.
   * Luckily std::string guarantees since C++11 that its contents is followed by a nullbyte
   * so all we have to do is increment the size
   */
  if (buf.starts_with("-----BEGIN ") && buf.back() != '\0') {
    dwSize++;
  }

  if (mbedtls_pk_parse_key(&mCtx, reinterpret_cast<const unsigned char *>(buf.c_str()), dwSize, nullptr, 0) == 0) {
    set = true;
    keyType = ASYMMETRIC_KEY_TYPE_PRIVATE;
  } else if (mbedtls_pk_parse_public_key(&mCtx, reinterpret_cast<const unsigned char *>(buf.c_str()), dwSize) == 0) {
    set = true;
    keyType = ASYMMETRIC_KEY_TYPE_PUBLIC;
  }
}

AsymmetricKey& AsymmetricKey::operator=(const AsymmetricKey& o) {
  if (&o != this) {
    std::lock_guard<std::mutex> lock(m);
    set = o.set;
    if (set) {
      keyType = o.keyType;
      int res;
      uint8_t buf[4096];

      switch (keyType) {
      case ASYMMETRIC_KEY_TYPE_PRIVATE:
        res = mbedtls_pk_write_key_der(&o.mCtx, buf, 4096);
        break;
      case ASYMMETRIC_KEY_TYPE_PUBLIC:
      default:
        res = mbedtls_pk_write_pubkey_der(&o.mCtx, buf, 4096);
        break;
      }

      if (res >= 0) {
        switch (keyType) {
        case ASYMMETRIC_KEY_TYPE_PRIVATE:
          res = mbedtls_pk_parse_key(
            &mCtx,
            &buf[4096 - res],
            static_cast<size_t>(res),
            nullptr,
            0
          );
          break;
        case ASYMMETRIC_KEY_TYPE_PUBLIC:
          res = mbedtls_pk_parse_public_key(
            &mCtx,
            &buf[4096 - res],
            static_cast<size_t>(res)
          );
          break;
        case ASYMMETRIC_KEY_TYPE_NONE:
          throw std::runtime_error("Asymmetric key type not set");
        }
      }

      if (res != 0) {
        // must never happen
        if(res < 0) {
          char error_buf[200];
          mbedtls_strerror( res, error_buf, 200 );
          throw std::runtime_error("must never happen; AsymmetricKey.cpp; operator=;  error code " + std::to_string(res) + ": " + error_buf);
        }
        throw std::runtime_error("must never happen; AsymmetricKey.cpp; operator=");
      }
    }
  }

  return *this;
}

bool AsymmetricKey::operator==(const AsymmetricKey& o) const {
  if (&o == this) {
    return true;
  }

  asymmetric_key_type_t own, other;
  if (toDer(&own) == o.toDer(&other)) {
    return own == other;
  }

  return false;
}


std::string AsymmetricKey::encrypt(const std::string& str) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set)
    throw std::runtime_error("AsymmetricKey::encrypt not set");
  std::string szOutput;
  szOutput.resize(MBEDTLS_MPI_MAX_SIZE);

  size_t olen = 0;

  if (const auto ret = mbedtls_pk_encrypt(
          &mCtx,
          reinterpret_cast<const unsigned char*>(str.data()),
          str.length(),
          reinterpret_cast<unsigned char*>(szOutput.data()),
          &olen,
          szOutput.length(),
          &MbedRandomSource,
          nullptr);
      ret == 0) {
    // Encryption successful
    szOutput.resize(olen);
  } else {
    char mbedErrorBuffer[1024] = {0};
    mbedtls_strerror(ret, mbedErrorBuffer, sizeof(mbedErrorBuffer));
    LOG(LOG_TAG, warning) << "AsymmetricKey::encrypt failure. ret = " << ret << ", mbedtls_strerror(ret) = " << mbedErrorBuffer;
    throw std::runtime_error("AsymmetricKey::encrypt failure.");
  }

  return szOutput;
}

std::string AsymmetricKey::decrypt(const std::string& str) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set)
    throw std::runtime_error("AsymmetricKey::decrypt not set");
  std::string szOutput;

  // Output should not become longer than input
  szOutput.resize(str.length());

  size_t olen = 0;

  if (const auto ret = mbedtls_pk_decrypt(
          &mCtx,
          reinterpret_cast<const unsigned char*>(str.data()),
          str.length(),
          reinterpret_cast<unsigned char*>(szOutput.data()),
          &olen,
          szOutput.length(),
          &MbedRandomSource,
          nullptr);
      ret == 0) {
    // Decryption successful
    szOutput.resize(olen);
  } else {
    char mbedErrorBuffer[1024] = {0};
    mbedtls_strerror(ret, mbedErrorBuffer, sizeof(mbedErrorBuffer));
    LOG(LOG_TAG, warning) << "AsymmetricKey::decrypt failure. ret = " << ret << ", mbedtls_strerror(ret) = " << mbedErrorBuffer;
    throw std::runtime_error("AsymmetricKey::decrypt failure.");
  }

  return szOutput;
}

std::string AsymmetricKey::toPem() const {
  std::lock_guard<std::mutex> lock(m);
  if (!set)  {
    return {};
  }
  char buf[4096] = {0};
  switch (keyType) {
  case ASYMMETRIC_KEY_TYPE_PRIVATE:
    mbedtls_pk_write_key_pem(&mCtx, reinterpret_cast<unsigned char*>(buf), 4096);
    break;
  case ASYMMETRIC_KEY_TYPE_PUBLIC:
    mbedtls_pk_write_pubkey_pem(&mCtx, reinterpret_cast<unsigned char*>(buf), 4096);
    break;
  case ASYMMETRIC_KEY_TYPE_NONE:
    throw std::runtime_error("Asymmetric key type not set");
  }

  return std::string(buf);
}

std::string AsymmetricKey::toDer(asymmetric_key_type_t* type) const {
  std::lock_guard<std::mutex> lock(m);

  if (type != nullptr) {
    *type = keyType;
  }
  if (!set) {
    return {};
  }

  const size_t BUFSIZE = 4096;
  char buffer[BUFSIZE];
  std::optional<int> lengthOpt;

  switch (keyType) {
  case ASYMMETRIC_KEY_TYPE_PRIVATE:
    lengthOpt = mbedtls_pk_write_key_der(&mCtx, reinterpret_cast<unsigned char *>(buffer), BUFSIZE);
    break;

  case ASYMMETRIC_KEY_TYPE_PUBLIC:
    lengthOpt = mbedtls_pk_write_pubkey_der(&mCtx, reinterpret_cast<unsigned char *>(buffer), BUFSIZE);
    break;

  case ASYMMETRIC_KEY_TYPE_NONE:
    throw std::runtime_error("Asymmetric key type not set");
  }

  if (!lengthOpt) {
    std::ostringstream error;
    error << "Unsupported key type " << keyType;
    throw std::runtime_error(error.str());
  }
  auto length = *lengthOpt;

  // Return value is "length of data written if successful, or a specific error code"
  if (length < 0 || static_cast<size_t>(length) > BUFSIZE) {
    std::ostringstream error;
    error << "Mbedtls failed to DER encode the asymmetric key with error code " << length;
    throw std::runtime_error(error.str());
  }

  /* Documentation for functions mbedtls_pk_write_key_der and mbedtls_pk_write_pubkey_der says:
   * Note: data is written at the end of the buffer! Use the return value to determine where you should start using the buffer.
   */
  auto start = buffer + BUFSIZE - length;
  return std::string(start, static_cast<size_t>(length));
}

bool AsymmetricKey::isPrivateKeyFor(const AsymmetricKey& publicKey) const {
  if (!set || !publicKey.set)
    return false;
  return mbedtls_pk_check_pair(&publicKey.mCtx, &mCtx) == 0;
}

std::string AsymmetricKey::signDigest(const std::string& abDigest) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set)
    throw std::runtime_error("AsymmetricKey::sign not set");
  std::string szOutput;
  size_t olen = 0;

  szOutput.resize(MBEDTLS_MPI_MAX_SIZE);

  if (const auto ret = mbedtls_pk_sign(
          &mCtx,
          MBEDTLS_MD_SHA256,
          reinterpret_cast<const unsigned char*>(abDigest.data()),
          abDigest.size(),
          reinterpret_cast<unsigned char*>(szOutput.data()),
          &olen,
          &MbedRandomSource,
          nullptr);
      ret == 0) {
    szOutput.resize(olen);
  }
  else {
    char mbedErrorBuffer[1024] = { 0 };
    mbedtls_strerror(ret, mbedErrorBuffer, sizeof(mbedErrorBuffer));
    LOG("AsymmetricKey", warning) << "AsymmetricKey::sign failure. ret = " << ret << ", mbedtls_strerror(ret) = " << mbedErrorBuffer;
    throw std::runtime_error("AsymmetricKey::sign failure.");
  }

  return szOutput;
}

bool AsymmetricKey::verifyDigest(const std::string& digest, const std::string& sig) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set)
    throw std::runtime_error("AsymmetricKey::verify not set");

  return mbedtls_pk_verify(
    &mCtx,
    MBEDTLS_MD_SHA256,
    reinterpret_cast<const unsigned char*>(digest.data()),
    digest.size(),
    reinterpret_cast<const unsigned char*>(sig.data()), sig.size()
  ) == 0;
}

AsymmetricKeyPair::AsymmetricKeyPair() {
  mbedtls_pk_init(&mCtx);
}

AsymmetricKeyPair::~AsymmetricKeyPair() {
  mbedtls_pk_free(&mCtx);
}

void AsymmetricKeyPair::generateKeyPair() {
  mbedtls_pk_context ctx;
  mbedtls_pk_init(&ctx);
  if (const auto ret = mbedtls_pk_setup(&ctx, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)); ret != 0) {
    LOG(LOG_TAG, warning) << "mbedtls_pk_setup failed (err=" << ret << ")";
    throw std::runtime_error("mbedtls_pk_setup failed");
  }

  mbedtls_rsa_init(mbedtls_pk_rsa(ctx), MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);

  if (const auto ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(ctx), MbedRandomSource, nullptr, 2048, 65537); ret != 0) {//TODO Check arguments, taken from X509Certificate.cpp:signCertificate
    LOG(LOG_TAG, critical) << "Generation of key pair failed (err=" << ret << ")";
    throw std::runtime_error("Generation of key pair failed");
  }

  if (const auto ret = mbedtls_pk_setup(&mCtx, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)); ret != 0) {
    LOG(LOG_TAG, warning) << "mbedtls_pk_setup failed (err=" << ret << ")";
    throw std::runtime_error("mbedtls_pk_setup failed");
  }

  if (const auto ret = mbedtls_rsa_copy(mbedtls_pk_rsa(mCtx), mbedtls_pk_rsa(ctx)); ret != 0) {
    LOG(LOG_TAG, warning) << "mbedtls_rsa_copy failed (err=" << ret << ")";
    throw std::runtime_error("mbedtls_rsa_copy failed");
  }

  mbedtls_pk_free(&ctx);
}

AsymmetricKey AsymmetricKeyPair::getPublicKey() {
  std::lock_guard<std::mutex> lock(m);
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, mCtx);
}

AsymmetricKey AsymmetricKeyPair::getPrivateKey() {
  std::lock_guard<std::mutex> lock(m);
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PRIVATE, mCtx);
}


}
