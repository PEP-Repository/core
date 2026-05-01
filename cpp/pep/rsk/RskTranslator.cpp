#include <pep/rsk/RskTranslator.hpp>

#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Hmac.hpp>
#include <pep/utils/OpenSSLHasher.hpp>

#include <stdexcept>
#include <utility>

using namespace pep;

namespace {
/// Check if \p encryption is nonzero
/// \throws std::invalid_argument for invalid encryption
/// \returns Reference to passed-in argument
const ElgamalEncryption& CheckValidEncryption(const ElgamalEncryption& encryption) {
  // Check used to be in AccessManager::handleEncryptionKeyRequest & Transcryptor::handleRekeyRequest
  if (encryption.getPublicKey().isZero()) {
    throw std::invalid_argument("ElgamalEncryption has zero public key");
  }
  ///NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter) Just use the function carefully
  return encryption;
}
} // namespace

RskTranslator::RskTranslator(Keys keys)
    : keys_(keys), cache_(&EGCache::get()) {}

CurveScalar RskTranslator::generateKeyFactor(const ReshuffleRecipient& recipient) const {
  const auto& key = keys_.reshuffle;
  if (!key) {
    throw std::logic_error("Reshuffle key is not set");
  }
  return generateKeyFactor(*key, recipient);
}

CurveScalar RskTranslator::generateKeyFactor(const RekeyRecipient& recipient) const {
  return generateKeyFactor(keys_.rekey, recipient);
}

RskTranslator::KeyFactors
RskTranslator::generateKeyFactors(const SkRecipient& recipient) const {
  return {
      .reshuffle = generateKeyFactor(static_cast<const ReshuffleRecipient&>(recipient)),
      .rekey = generateKeyFactor(static_cast<const RekeyRecipient&>(recipient)),
  };
}

ElgamalEncryption RskTranslator::rsk(
    const ElgamalEncryption& encryption,
    const KeyFactors& recipientKeyFactors) const {
  return cache_->rsk(
      CheckValidEncryption(encryption),
      recipientKeyFactors.reshuffle,
      recipientKeyFactors.rekey);
}

ElgamalEncryption RskTranslator::rk(
    const ElgamalEncryption& encryption,
    const ElgamalTranslationKey& recipientRekeyKeyFactor) const {
  return cache_->rk(
      CheckValidEncryption(encryption),
      recipientRekeyKeyFactor);
}

ElgamalEncryption RskTranslator::rs(
    const ElgamalEncryption& encryption,
    const CurveScalar& recipientReshuffleKeyFactor) const {
  //TODO This could potentially be cached?
  return CheckValidEncryption(encryption).rerandomize().reshuffle(recipientReshuffleKeyFactor);
}

std::pair<ElgamalEncryption, RskProof> RskTranslator::certifiedRsk(
    const ElgamalEncryption& encryption,
    const KeyFactors& recipientKeyFactors) const {
  ElgamalEncryption encryptionOut;
  //TODO Why is this not cached?
  RskProof proofOut = RskProof::CertifiedRsk(
      CheckValidEncryption(encryption), encryptionOut,
      recipientKeyFactors.reshuffle,
      recipientKeyFactors.rekey);
  return {encryptionOut, proofOut};
}

ReshuffleRekeyVerifiers RskTranslator::computeReshuffleRekeyVerifiers(
    const KeyFactors& recipientKeyFactors,
    const ElgamalPublicKey& masterPublicEncryptionKey) const {
  return ReshuffleRekeyVerifiers::Compute(
      recipientKeyFactors.reshuffle,
      recipientKeyFactors.rekey,
      masterPublicEncryptionKey);
}

CurveScalar RskTranslator::generateKeyComponent(
    const CurveScalar& encryptionKeyFactor,
    const CurveScalar& masterPrivateEncryptionKeyShare
) const {
  return encryptionKeyFactor * masterPrivateEncryptionKeyShare;
}

/// Generate a key factor
/// \details Depending on the KeyFactorDomain, we generate a pseudonym or data key factor
/// \param keyFactorSecret A key factor secret
/// \param recipient Recipient
/// \returns Key factor
/// \note This function does not work for data key blinding, as the HMAC is computed differently
CurveScalar RskTranslator::generateKeyFactor(
    const KeyFactorSecret& keyFactorSecret,
    const RecipientBase& recipient) const {
  if (!keys_.domain) {
    throw std::invalid_argument("Key domain is not set");
  }
  Sha256 hasher;
  hasher.update(PackUint32BE(keys_.domain));
  hasher.update(PackUint32BE(recipient.type()));
  auto digest = hasher.digest(recipient.payload());
  return CurveScalar::From64Bytes(Hmac<Sha512>(SpanToString(keyFactorSecret.hmacKey()), digest));
}
