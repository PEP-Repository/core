#pragma once

#include <mutex>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>

#include <pep/crypto/Timestamp.hpp>

namespace pep {

enum asymmetric_key_type_t {
  ASYMMETRIC_KEY_TYPE_NONE = 0,
  ASYMMETRIC_KEY_TYPE_PUBLIC,
  ASYMMETRIC_KEY_TYPE_PRIVATE,
};

class AsymmetricKey {
 public:
  std::string signDigest(const std::string& abDigest) const;
  bool verifyDigest(const std::string& digest, const std::string& sig) const;

  AsymmetricKey();
  AsymmetricKey(const std::string& buf);
  AsymmetricKey(const AsymmetricKey& o);
  AsymmetricKey& operator=(const AsymmetricKey& o);
  bool isPrivateKeyFor(const AsymmetricKey& publicKey) const;
  ~AsymmetricKey();

  bool operator==(const AsymmetricKey& o) const;
  inline bool operator!=(const AsymmetricKey& o) const { return !(*this == o); }

  std::string toPem() const;
  std::string toDer(asymmetric_key_type_t* type = nullptr) const;

  std::string encrypt(const std::string& str) const;
  std::string decrypt(const std::string& str) const;

  asymmetric_key_type_t keyType = ASYMMETRIC_KEY_TYPE_NONE;

  bool isSet() const {
    return set;
  }

 protected:
  mutable mbedtls_pk_context mCtx;
  AsymmetricKey(asymmetric_key_type_t keyType, mbedtls_pk_context& o);
  bool set = false;
  mutable std::mutex m;

  friend class X509Certificate;
  friend class X509CertificateSigningRequest;
  friend class AsymmetricKeyPair;
};

class AsymmetricKeyPair {
 public:
  AsymmetricKeyPair();
  ~AsymmetricKeyPair();

  AsymmetricKey getPublicKey();
  AsymmetricKey getPrivateKey();

  void generateKeyPair();

 protected:
  mbedtls_pk_context mCtx;
  std::mutex m;

  friend class X509CertificateSigningRequest;
};
}
