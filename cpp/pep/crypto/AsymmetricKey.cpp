#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <boost/numeric/conversion/cast.hpp>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/OpensslUtils.hpp>
#include <pep/utils/Defer.hpp>

#include <string>
#include <mutex>
#include <string_view>
#include <stdexcept>

namespace pep {

static const std::string LOG_TAG ("AsymmetricKey");

AsymmetricKey::~AsymmetricKey() {
  EVP_PKEY_free(mKey);
}

/**
 * @brief Constructor. Initializes the AsymmetricKey object with the given key type makes a deep copy of the given key.
 * @param keyType The type of the key (private or public).
 * @param o The EVP_PKEY* representing the key.
 */
AsymmetricKey::AsymmetricKey(asymmetric_key_type_t keyType, EVP_PKEY* o) {
  this->keyType = keyType;

  if (keyType != ASYMMETRIC_KEY_TYPE_NONE) {
    if (o != nullptr) {
      mKey = EVP_PKEY_dup(o);
      if (mKey != nullptr) {
        set = true;
      } else {
        throw pep::OpenSSLError("Failed to duplicate given EVP_PKEY in AsymmetricKey constructor.");
      }
    } else {
      throw std::invalid_argument("Input EVP_PKEY is nullptr in AsymmetricKey constructor.");
    }
  } else {
    throw std::invalid_argument("Asymmetric key type not set in AsymmetricKey constructor.");
  }
}

/**
 * @brief Constructor. Initializes the AsymmetricKey object with the key data from the given buffer.
 * @param buf The buffer containing the key data in PEM format.
 */
AsymmetricKey::AsymmetricKey(std::string_view buf) {

  BIO* bio = BIO_new_mem_buf(buf.data(), boost::numeric_cast<int>(buf.size()));
  if (!bio) {
    throw pep::OpenSSLError("Failed to create IO buffer (BIO) in AsymmetricKey constructor.");
  }
  PEP_DEFER(BIO_free(bio));

  // Attempt to read a private key, pass in pointer to newly created EVP_PKEY object, returns nullptr on failure
  if (!PEM_read_bio_PrivateKey(bio, &mKey, nullptr, nullptr)) {
    // If reading the private key fails, attempt to read as a public key
    // If both attempts fail, do nothing, which is what the previous mbed implementation did for some reason
    BIO_reset(bio); // Reset BIO to attempt to read again
    ERR_clear_error(); // clear error queue
    if (PEM_read_bio_PUBKEY(bio, &mKey, nullptr, nullptr)) {
      set = true;
      keyType = ASYMMETRIC_KEY_TYPE_PUBLIC;
    } else {
      throw pep::OpenSSLError("Failed to read key from IO buffer (BIO) in AsymmetricKey constructor.");
    }
  } else {
    set = true;
    keyType = ASYMMETRIC_KEY_TYPE_PRIVATE;
  }
}

AsymmetricKey::AsymmetricKey(const AsymmetricKey& other) {
  std::lock_guard<std::mutex> lock(other.m);
  if (other.mKey) {
    mKey = EVP_PKEY_dup(other.mKey);
    if (!mKey) {
      throw pep::OpenSSLError("Failed to duplicate given EVP_PKEY in AsymmetricKey copy constructor.");
    }
    set = other.set;
    keyType = other.keyType;
  }
}

AsymmetricKey::AsymmetricKey(AsymmetricKey&& other) {
  std::lock_guard<std::mutex> lock(other.m);
  if (other.mKey) {
    mKey = std::exchange(other.mKey, nullptr);
    set = std::exchange(other.set, false);
    keyType = std::exchange(other.keyType, ASYMMETRIC_KEY_TYPE_NONE);
  }
}

AsymmetricKey& AsymmetricKey::operator=(AsymmetricKey other) {
  std::lock_guard<std::mutex> lock(m);
  std::swap(mKey, other.mKey);
  set = other.set;
  keyType = other.keyType;
  return *this;
}

bool AsymmetricKey::operator==(const AsymmetricKey& other) const {
  if (&other == this) {
    return true;
  }

  // Lock both mutexes without deadlock
  std::scoped_lock lock(m, other.m);

  if (keyType != other.keyType) {
    return false;
  }

  int result = EVP_PKEY_eq(mKey, other.mKey);
  if (result == 1) {
    return true;
  } else if (result == 0) {
    return false;
  } else if (result == -1) {
    LOG(LOG_TAG, pep::error) << "Key types are different in AsymmetricKey::operator==.";
    return false;
  } else if (result == -2) {
    throw pep::OpenSSLError("EVP_PKEY_eq operation not supported.");
  }

  return false;
}

std::string AsymmetricKey::encrypt(const std::string& str) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set) {
    throw std::runtime_error("Failure trying to encrypt with key not set in AsymmetricKey::encrypt.");
  }

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(mKey, nullptr);
  if (!ctx) {
    throw pep::OpenSSLError("Failed to create EVP_PKEY_CTX in AsymmetricKey::encrypt.");
  }
  PEP_DEFER(EVP_PKEY_CTX_free(ctx));

  // Initialize the encryption context
  if (EVP_PKEY_encrypt_init(ctx) <= 0) {
    throw pep::OpenSSLError("Failed to set up key context in AsymmetricKey::encrypt.");
  }

  // Determine the max size of the output buffer
  std::size_t outlen{};
  if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, reinterpret_cast<const unsigned char*>(str.data()), str.length()) <= 0) {
    throw pep::OpenSSLError("Failure to determine output buffer size in AsymmetricKey::encrypt.");
  }

  // Encrypt the data
  std::string szOutput;
  szOutput.resize(outlen);
  if (EVP_PKEY_encrypt(ctx, reinterpret_cast<unsigned char*>(szOutput.data()), &outlen, reinterpret_cast<const unsigned char*>(str.data()), str.length()) <= 0) {
    throw pep::OpenSSLError("Encrypt failure in AsymmetricKey::encrypt.");
  }

  // Resize the output buffer to the actual size
  szOutput.resize(outlen);
  return szOutput;
}

std::string AsymmetricKey::decrypt(const std::string& str) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set) {
    throw std::runtime_error("AsymmetricKey not set");
  }

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(mKey, nullptr);
  if (!ctx) {
    throw pep::OpenSSLError("Failed to create EVP_PKEY_CTX in AsymmetricKey::decrypt.");
  }
  PEP_DEFER(EVP_PKEY_CTX_free(ctx));

  if (EVP_PKEY_decrypt_init(ctx) <= 0) {
    throw pep::OpenSSLError("Failed to initialize EVP_PKEY_CTX in AsymmetricKey::decrypt.");
  }

  size_t outlen{};
  if (EVP_PKEY_decrypt(ctx, nullptr, &outlen, reinterpret_cast<const unsigned char*>(str.data()), str.length()) <= 0) {
    throw pep::OpenSSLError("Failed to obtain decrypted size in AsymmetricKey::decrypt.");
  }

  std::string szOutput;
  szOutput.resize(outlen);
  if (EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char*>(szOutput.data()), &outlen, reinterpret_cast<const unsigned char*>(str.data()), str.length()) <= 0) {
    throw pep::OpenSSLError("Failed to decrypt in AsymmetricKey::decrypt.");
  }

  szOutput.resize(outlen);
  return szOutput;
}

/**
 *
 * Write a public/private key to a PKCS#8 PEM string.
 *
 * @brief Converts the key to PEM format.
 * @return The key in PEM format.
 * @throws std::runtime_error if the key type is not set or if an OpenSSL error occurred.
 */
std::string AsymmetricKey::toPem() const {
  std::lock_guard<std::mutex> lock(m);
  if (!set) {
    return {};
  }

  BIO* bio = nullptr;
  PEP_DEFER(BIO_free(bio));

  // Write the key to the BIO
  switch (keyType) {
  case ASYMMETRIC_KEY_TYPE_PRIVATE:
    bio = BIO_new(BIO_s_secmem());
    if (!bio) {
      throw pep::OpenSSLError("Failed to create secure memory IO buffer (BIO) in AsymmetricKey::toPem.");
    }
    if (PEM_write_bio_PrivateKey(bio, mKey, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
      throw pep::OpenSSLError("Failed to write private key to IO buffer (BIO) in AsymmetricKey::toPem.");
    }
    break;
  case ASYMMETRIC_KEY_TYPE_PUBLIC:
    bio = BIO_new(BIO_s_mem());
    if (!bio) {
      throw pep::OpenSSLError("Failed to create IO buffer (BIO) in AsymmetricKey::toPem.");
    }
    if (PEM_write_bio_PUBKEY(bio, mKey) <= 0) {
      throw pep::OpenSSLError("Failed to write public key to IO buffer (BIO) in AsymmetricKey::toPem.");
    }
    break;
  case ASYMMETRIC_KEY_TYPE_NONE:
    throw std::runtime_error("Failed to write key to PEM in AsymmetricKey::toPem. Asymmetric key type not set");
  }
  if (!bio) {
    throw std::invalid_argument("Unsupported ASYMMETRIC_KEY_TYPE in AsymmetricKey::toPem.");
  }

  return OpenSSLBIOToString(bio);
}

std::string AsymmetricKey::toDer() const {
  std::lock_guard<std::mutex> lock(m);

  if (!set) {
    return {};
  }

  BIO* bio = nullptr;
  PEP_DEFER(BIO_free(bio));

  // Write the key to the BIO
  switch (keyType) {
    case ASYMMETRIC_KEY_TYPE_PRIVATE:
      bio = BIO_new(BIO_s_secmem());
      if (!bio) {
        throw pep::OpenSSLError("Failed to create secure memory IO buffer (BIO) in AsymmetricKey::toDer.");
      }
      if (i2d_PKCS8PrivateKey_bio(bio, mKey, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        throw pep::OpenSSLError("Failed to write private key to IO buffer (BIO) in AsymmetricKey::toDer.");
      }
      break;
    case ASYMMETRIC_KEY_TYPE_PUBLIC:
      bio = BIO_new(BIO_s_mem());
      if (!bio) {
        throw pep::OpenSSLError("Failed to create IO buffer (BIO) in AsymmetricKey::toDer.");
      }
      if (i2d_PUBKEY_bio(bio, mKey) <= 0) {
        throw pep::OpenSSLError("Failed to write public key to IO buffer (BIO) in AsymmetricKey::toDer.");
      }
      break;
    case ASYMMETRIC_KEY_TYPE_NONE:
      throw std::invalid_argument("Failure to write key to DER in AsymmetricKey::toDer. Asymmetric key type not set.");
  }
  if (!bio) {
    throw std::invalid_argument("Failure to write key to DER in AsymmetricKey::toDer. Unsupported key type " + std::to_string(keyType));
  }

  return OpenSSLBIOToString(bio);
}

/**
 * @brief Checks if this private key corresponds to the given public key.
 * @param publicKey The public key to be checked.
 * @return True if the private key corresponds to the public key, false otherwise.
 */
bool AsymmetricKey::isPrivateKeyFor(const AsymmetricKey& publicKey) const {
  std::scoped_lock lock(m, publicKey.m);
  if (!set || !publicKey.set){
    return false;
  }

  // Ensure that this key is a private key and publicKey is a public key
  if (keyType != ASYMMETRIC_KEY_TYPE_PRIVATE || publicKey.keyType != ASYMMETRIC_KEY_TYPE_PUBLIC) {
    return false;
  }

  // From https://stackoverflow.com/a/73021315
  // We will compare the modulus (n) of the RSA keys to determine if they correspond.
  // In RSA, the modulus (n) is a part of both the public and private keys.
  // The modulus (n) is the product of two large prime numbers (p and q) and is used in both the encryption and decryption processes.
  // If the modulus (n) of the private key matches the modulus (n) of the public key, it indicates that the keys are a pair.
  // This is because the modulus (n) is unique to the key pair and is derived from the same prime factors (p and q).

  BIGNUM* rsa_pub_n = nullptr;
  PEP_DEFER(BN_free(rsa_pub_n));

  BIGNUM* rsa_priv_n = nullptr;
  PEP_DEFER(BN_free(rsa_priv_n));

  // Extract n with EVP_PKEY_get_bn_param for private key
  if (EVP_PKEY_is_a(mKey, "RSA")) {
    if (EVP_PKEY_get_bn_param(mKey, OSSL_PKEY_PARAM_RSA_N, &rsa_priv_n) <= 0) {
      throw pep::OpenSSLError("Failed to get RSA private key n in AsymmetricKey::isPrivateKeyFor.");
    }
  } else {
    throw std::invalid_argument("Failure. Private key is not RSA in AsymmetricKey::isPrivateKeyFor.");
  }

  // Extract the modulus (n) from the public key
  if (EVP_PKEY_is_a(publicKey.mKey, "RSA")) {
    if (EVP_PKEY_get_bn_param(publicKey.mKey, OSSL_PKEY_PARAM_RSA_N, &rsa_pub_n) <= 0) {
      throw pep::OpenSSLError("Failed to get RSA public key n in AsymmetricKey::isPrivateKeyFor.");
    }
  } else {
    throw std::invalid_argument("Failure. Public key is not RSA in AsymmetricKey::isPrivateKeyFor.");
  }

  // Compare the modulus (n) of the private key and the public key
  if (BN_cmp(rsa_priv_n, rsa_pub_n) == 0) {
    // The modulus (n) is the same, so the private key corresponds to the public key
    return true;
  } else {
    return false;
  }
}

std::string AsymmetricKey::signDigestSha256(const std::string& abDigest) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set) {
    throw std::runtime_error("Failure in AsymmetricKey::signDigestSha256. AsymmetricKey not set.");
  }

  // Create a new signature context
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(mKey, nullptr);
  if (!ctx) {
    throw pep::OpenSSLError("Failed to create signature context in AsymmetricKey::signDigestSha256.");
  }
  PEP_DEFER(EVP_PKEY_CTX_free(ctx));

  // Initialize the signature context
  if (EVP_PKEY_sign_init(ctx) <= 0) {
    throw pep::OpenSSLError("Failed to initialize signature context in AsymmetricKey::signDigestSha256.");
  }

  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
    throw pep::OpenSSLError("Failed to set RSA padding in AsymmetricKey::signDigestSha256.");
  }

  if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0){
    throw pep::OpenSSLError("Failed to set signature MD to sha256 in AsymmetricKey::signDigestSha256.");
  }

  // Determine the buffer length for the signature
  size_t sigLen{};
  if (EVP_PKEY_sign(ctx, nullptr, &sigLen, reinterpret_cast<const unsigned char*>(abDigest.data()), abDigest.size()) <= 0) {
    throw pep::OpenSSLError("Failed to determine signature length in AsymmetricKey::signDigestSha256.");
  }

  std::string sig;
  sig.resize(sigLen);

  // Generate the signature
  if (EVP_PKEY_sign(ctx, reinterpret_cast<unsigned char*>(sig.data()), &sigLen, reinterpret_cast<const unsigned char*>(abDigest.data()), abDigest.size()) <= 0) {
    throw pep::OpenSSLError("Failed to sign digest in AsymmetricKey::signDigestSha256.");
  }

  sig.resize(sigLen);
  return sig;
}

bool AsymmetricKey::verifyDigestSha256(const std::string& digest, const std::string& sig) const {
  std::lock_guard<std::mutex> lock(m);
  if (!set) {
    throw std::runtime_error("Failure in AsymmetricKey::verifyDigestSha256. AsymmetricKey not set.");
  }

  // Create a new signature verification context
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(mKey, nullptr);
  if (!ctx) {
    throw pep::OpenSSLError("Failed to create EVP_PKEY_CTX in AsymmetricKey::verifyDigestSha256.");
  }
  PEP_DEFER(EVP_PKEY_CTX_free(ctx));

  // Initialize the verification context
  if (EVP_PKEY_verify_init(ctx) <= 0) {
    throw pep::OpenSSLError("EVP_PKEY_verify_init failed in AsymmetricKey::verifyDigestSha256.");
  }

  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
    throw pep::OpenSSLError("Failed to set RSA padding in AsymmetricKey::verifyDigestSha256.");
  }

  if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0){
    throw pep::OpenSSLError("EVP_PKEY_set_signature failed in AsymmetricKey::verifyDigestSha256.");
  }

  // Perform the raw verification
  int verifyResult = EVP_PKEY_verify(ctx, reinterpret_cast<const unsigned char*>(sig.data()), sig.size(), reinterpret_cast<const unsigned char*>(digest.data()), digest.size());

  if (verifyResult == 1) {
    return true;
  } else if (verifyResult == 0) {
    // Signature verification failed
    auto errors = pep::TakeOpenSSLErrors();
    LOG(LOG_TAG, error) << "Failure to verify signature in AsymmetricKey::verifyDigestSha256." << errors;
    return false;
  } else {
    // Some other error occurred
    throw pep::OpenSSLError("EVP_PKEY_verify failed in AsymmetricKey::verifyDigestSha256.");
  }
}

AsymmetricKeyPair::AsymmetricKeyPair(const AsymmetricKeyPair& other) {
  std::lock_guard<std::mutex> lock(other.m);
  if (other.mKeyPair) {
    mKeyPair = EVP_PKEY_dup(other.mKeyPair);
    if (!mKeyPair) {
      throw pep::OpenSSLError("Failed to duplicate key pair in AsymmetricKeyPair copy constructor.");
    }
  }
}

AsymmetricKeyPair& AsymmetricKeyPair::operator=(AsymmetricKeyPair other) {
  std::lock_guard<std::mutex> lock(m);
  std::swap(mKeyPair, other.mKeyPair);
  return *this;
}

AsymmetricKeyPair::~AsymmetricKeyPair() {
  EVP_PKEY_free(mKeyPair);
}

AsymmetricKeyPair AsymmetricKeyPair::GenerateKeyPair() {
  AsymmetricKeyPair keyPair;
  keyPair.mKeyPair = EVP_RSA_gen(2048);
  if (!keyPair.mKeyPair) {
    throw pep::OpenSSLError("Failed to generate key pair in AsymmetricKeyPair.");
  }
  return keyPair;
}

AsymmetricKey AsymmetricKeyPair::getPublicKey() const {
  std::lock_guard<std::mutex> lock(m);
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PUBLIC, mKeyPair);
}

AsymmetricKey AsymmetricKeyPair::getPrivateKey() const {
  std::lock_guard<std::mutex> lock(m);
  return AsymmetricKey(ASYMMETRIC_KEY_TYPE_PRIVATE, mKeyPair);
}

} // namespace pep
