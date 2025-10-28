#include <pep/crypto/CPRNG.hpp>
#include <stdexcept>

namespace {
pep::EvpContext newEvpContext() {
  return pep::EvpContext(EVP_CIPHER_CTX_new(),
    [](EVP_CIPHER_CTX* ctx) {
      EVP_CIPHER_CTX_free(ctx);
    });
}
} // namespace

namespace pep {

CPURBG::CPURBG()
    : mCtx(newEvpContext()) {
  std::string seed;
  RandomBytes(seed, 32 + 16); // 32 byte key + 16 byte IV
  if (1 != EVP_EncryptInit_ex(
        mCtx.get(),
        EVP_aes_256_cbc(),
        nullptr,
        reinterpret_cast<uint8_t*>(seed.data()),
        reinterpret_cast<uint8_t*>(seed.data() + 32)))
    throw std::runtime_error("CPURBG: EVP_EncryptInit_ex failed");
}

void CPURBG::fillBuffer() {
  mBufferIdx = 0;
  std::array<uint64_t, bufferSize> plaintext{};
  plaintext[0] = mFillCount++;
  int len{};
  if (1 != EVP_EncryptUpdate(
        mCtx.get(),
        reinterpret_cast<uint8_t*>(mBuffer.data()),
        &len,
        reinterpret_cast<const uint8_t*>(plaintext.data()),
        bufferSize*8))
    throw std::runtime_error("CPURBG: EVP_EncryptUpdate failed");
  if (len != bufferSize*8)
    throw std::runtime_error("CPURBG: EVP_EncryptUpdate behaved unexpectedly");
}

CPRNG::CPRNG()
    : mCtx(newEvpContext()) {
  std::string seed;
  RandomBytes(seed, 32 + 16);
  if (1 != EVP_EncryptInit_ex(
        mCtx.get(),
        EVP_aes_256_cbc(),
        nullptr,
        reinterpret_cast<const uint8_t*>(seed.data()),
        reinterpret_cast<const uint8_t*>(seed.data() + 32)))
    throw std::runtime_error("CPRNG: EVP_EncryptInit_ex failed");
}

void CPRNG::operator()(std::span<uint8_t> buffer) {
  std::lock_guard<std::mutex> g(mLock);
  std::array<uint8_t, 64> plaintext{};
  int outLen{};
  auto offsetBuffer = buffer;
  while (offsetBuffer.size() >= 64) {
    if (1 != EVP_EncryptUpdate(
          mCtx.get(),
          offsetBuffer.data(),
          &outLen,
          plaintext.data(),
          64))
      throw std::runtime_error("CPRNG: EVP_EncryptUpdate failed");
    if (outLen != 64)
      throw std::runtime_error("CPRNG: EVP_EncryptUpdate behaved unexpectedly");
    offsetBuffer = offsetBuffer.subspan(64);
  }
  if (offsetBuffer.empty())
    return;
  std::array<uint8_t, 64> tmp{};
  if (1 != EVP_EncryptUpdate(
        mCtx.get(),
        tmp.data(),
        &outLen,
        plaintext.data(),
        64))
    throw std::runtime_error("CPRNG: EVP_EncryptUpdate failed");
  if (outLen != 64)
    throw std::runtime_error("CPRNG: EVP_EncryptUpdate behaved unexpectedly");
  std::ranges::copy(std::span(tmp).subspan(0, offsetBuffer.size()), offsetBuffer.begin());
}

}
