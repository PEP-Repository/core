#pragma once

#include <openssl/evp.h>

#include <mutex>
#include <string>
#include <string_view>

namespace pep {

enum asymmetric_key_type_t {
  ASYMMETRIC_KEY_TYPE_NONE = 0,
  ASYMMETRIC_KEY_TYPE_PUBLIC,
  ASYMMETRIC_KEY_TYPE_PRIVATE,
};

class AsymmetricKey {
 public:
  std::string signDigestSha256(const std::string& abDigest) const;
  [[nodiscard]] bool verifyDigestSha256(const std::string& digest, const std::string& sig) const;

  AsymmetricKey() = default;
  explicit AsymmetricKey(std::string_view buf);
  AsymmetricKey(const AsymmetricKey& other);
  AsymmetricKey& operator=(AsymmetricKey other);
  bool isPrivateKeyFor(const AsymmetricKey& publicKey) const;
  ~AsymmetricKey();

  bool operator==(const AsymmetricKey& other) const;
  inline bool operator!=(const AsymmetricKey& other) const { return !(*this == other); }

  std::string toPem() const;
  std::string toDer() const;

  std::string encrypt(const std::string& str) const;
  std::string decrypt(const std::string& str) const;

  asymmetric_key_type_t keyType = ASYMMETRIC_KEY_TYPE_NONE;

  bool isSet() const {
    return set;
  }

 private:
  EVP_PKEY* mKey = nullptr;
  AsymmetricKey(asymmetric_key_type_t keyType, EVP_PKEY* o);
  bool set = false;
  mutable std::mutex m;

  friend class X509Certificate;
  friend class X509CertificateSigningRequest;
  friend class AsymmetricKeyPair;
};

class AsymmetricKeyPair {
 public:
  AsymmetricKeyPair() = default;
  AsymmetricKeyPair(const AsymmetricKeyPair& other);
  AsymmetricKeyPair& operator=(AsymmetricKeyPair other);
  ~AsymmetricKeyPair();

  static AsymmetricKeyPair GenerateKeyPair();
  
  AsymmetricKey getPublicKey();
  AsymmetricKey getPrivateKey();

 private:
  EVP_PKEY* mKeyPair = nullptr;
  mutable std::mutex m;

  friend class X509CertificateSigningRequest;
};
}
