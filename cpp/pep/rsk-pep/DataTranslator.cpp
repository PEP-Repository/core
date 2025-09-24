#include <pep/rsk-pep/DataTranslator.hpp>

#include <pep/rsk-pep/KeyDomain.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Sha.hpp>

#include <stdexcept>

using namespace pep;

DataTranslator::DataTranslator(DataTranslationKeys keys)
    : rsk_({
               .domain = static_cast<RskTranslator::KeyDomainType>(KeyDomain::Data),
               .reshuffle = keys.blindingKeySecret,
               .rekey = keys.encryptionKeyFactorSecret,
           }),
      masterPrivateEncryptionKeyShare_(keys.masterPrivateEncryptionKeyShare) {}

/// Generate blinding key (reshuffle)
/// \param blindMode Generate a blinding or unblinding key?
/// \param blindAddData See Metadata::computeKeyBlindingAdditionalData
/// \param invertBlindKey Invert the blinding key instead of the unblinding key? (New behavior)
/// \returns Blinding key
CurveScalar DataTranslator::generateBlindingKey(
    BlindMode blindMode,
    std::string_view blindAddData,
    bool invertBlindKey
) const {
  const auto& blindingKeySecret = rsk_.keys().reshuffle;
  if (!blindingKeySecret) {
    throw std::logic_error("Trying to perform key unblinding while blinding key is not set (only AM should call this)");
  }
  auto key = CurveScalar::From64Bytes(Sha512::HMac(
      SpanToString(blindingKeySecret->hmacKey()),
      blindAddData));
  // If invertBlindKey, we invert the blinding key. Otherwise, we invert the unblinding key
  if (invertBlindKey == (blindMode == BlindMode::Blind)) {
    key = key.invert();
  }
  return key;
}

// Public interface: doc comments in declaration

ElgamalEncryption DataTranslator::blind(
    const ElgamalEncryption& unblinded,
    std::string_view blindAddData,
    bool invertBlindKey
) const {
  return rsk_.rs(unblinded, generateBlindingKey(BlindMode::Blind, blindAddData, invertBlindKey));
}

ElgamalEncryption DataTranslator::unblindAndTranslate(
    const ElgamalEncryption& blinded,
    std::string_view blindingAddData,
    bool invertBlindKey,
    const Recipient& recipient
) const {
  return rsk_.rsk(blinded, {
      .reshuffle = generateBlindingKey(BlindMode::Unblind, blindingAddData, invertBlindKey),
      .rekey = rsk_.generateKeyFactor(recipient),
  });
}

ElgamalEncryption DataTranslator::translateStep(
    const ElgamalEncryption& encrypted,
    const Recipient& recipient
) const {
  return rsk_.rk(encrypted, rsk_.generateKeyFactor(recipient));
}

CurveScalar DataTranslator::generateKeyComponent(const Recipient& recipient) const {
  return rsk_.generateKeyComponent(
      rsk_.generateKeyFactor(recipient),
      CurveScalar(SpanToString(masterPrivateEncryptionKeyShare_.curveScalar())));
}
