#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <string>

#include <openssl/evp.h>

#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Hasher.hpp>

namespace pep {

namespace detail {

template <class TDerived, size_t OUTPUTSIZE, size_t BLOCKSIZE>
class Sha : public Hasher<std::string> {
public:
  using typename Hasher<std::string>::Hash;

private:
  EVP_MD_CTX* mdctx = nullptr;

protected:
  Sha() = default;

  Hash finish() override {
    std::string ret(OUTPUTSIZE, '\0');
    digestInto(ConvertBytes<std::uint8_t>(std::span<char, OUTPUTSIZE>(ret)));
    return ret;
  }

  virtual const EVP_MD* getDigestType() const = 0;

private:
  // We can't initialize in the constructor because we use the virtual method "getDigestType"
  void ensureInitialized() {
    if (mdctx == nullptr) {
      mdctx = EVP_MD_CTX_new();
      EVP_DigestInit_ex(mdctx, this->getDigestType(), nullptr);
    }
  }

  void digestInto(std::span<std::uint8_t, OUTPUTSIZE> out) {
    this->ensureInitialized();
    unsigned int s{};
    EVP_DigestFinal_ex(mdctx, out.data(), &s);
    assert(s == out.size());
  }

protected:
  void process(const void* block, size_t size) override {
    this->ensureInitialized();
    EVP_DigestUpdate(mdctx, block, size);
  }

public:
  ~Sha() noexcept override {
    if (mdctx != nullptr) {
      EVP_MD_CTX_free(mdctx);
    }
  }

public:
  static std::string HMac(std::string_view key, std::string_view data);
};

/*!
 * \brief Compute HMAC using SHA according to RFC 2104
 * \return HMAC
 */
template <class TDerived, size_t OUTPUTSIZE, size_t BLOCKSIZE>
std::string Sha<TDerived, OUTPUTSIZE, BLOCKSIZE>::HMac(std::string_view key, std::string_view data) {
  std::array<uint8_t, BLOCKSIZE> k{};

  // Check if key is longer than blocksize
  if (key.size() > BLOCKSIZE) {
    // Shorten key by hashing it
    TDerived sha;
    sha.update(key);
    static_cast<Sha&>(sha).digestInto(std::span(k).template subspan<0, OUTPUTSIZE>());
  }
  else {
    // Copy the provided key as is. Because of the initial value of k, it will be zero padded
    std::ranges::copy(key, k.begin());
  }

  // Construct padded keys
  //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) Fully overwritten below
  std::array<uint8_t, BLOCKSIZE> k_ipad, k_opad;
  for (unsigned int i = 0; i < BLOCKSIZE; i++) {
    k_ipad[i] = k[i] ^ 0x36;
    k_opad[i] = k[i] ^ 0x5C;
  }

  // Compute inner hash
  //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) Fully overwritten by digestInto
  std::array<uint8_t, OUTPUTSIZE> intermediate_hash;
  TDerived sha;
  sha
    .update(k_ipad.data(), BLOCKSIZE)
    .update(data);
  static_cast<Sha&>(sha).digestInto(intermediate_hash);

  // Compute final HMAC
  return TDerived()
    .update(k_opad.data(), BLOCKSIZE)
    .update(intermediate_hash.data(), OUTPUTSIZE)
    .digest();
}

} // namespace detail

class Sha256 : public detail::Sha<Sha256, 32, 64> {
protected:
  const EVP_MD* getDigestType() const override { return EVP_sha256(); }
};

class Sha512 : public detail::Sha<Sha512, 64, 128> {
protected:
  const EVP_MD* getDigestType() const override { return EVP_sha512(); }
};

}
