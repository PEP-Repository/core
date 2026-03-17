#pragma once

#include <pep/elgamal/CurveScalar.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/rsk/RskKeys.hpp>
#include <pep/rsk/RskRecipient.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace pep {

class RskTranslator {
public:
  using KeyDomainType = uint32_t;

  struct Keys {
    KeyDomainType domain;
    std::optional<KeyFactorSecret> reshuffle;
    KeyFactorSecret rekey;

    Keys(KeyDomainType domain_, std::optional<KeyFactorSecret> reshuffle_, KeyFactorSecret rekey_)
      : domain(domain_), reshuffle(std::move(reshuffle_)), rekey(std::move(rekey_)) {
    }
  };

  struct KeyFactors {
    /// I.e. pseudonymization / blinding
    CurveScalar reshuffle;
    /// Encryption
    ElgamalTranslationKey rekey;
  };

  explicit RskTranslator(Keys keys);

  [[nodiscard]] const Keys& keys() const { return keys_; }

  /// Generate a reshuffle key factor
  /// \note This function does not work for data key blinding, as the HMAC is computed differently
  /// \throws std::logic_error if reshuffle key is not set
  [[nodiscard]] CurveScalar generateKeyFactor(const ReshuffleRecipient& recipient) const;

  /// Generate a rekey key factor
  [[nodiscard]] ElgamalTranslationKey generateKeyFactor(const RekeyRecipient& recipient) const;

  /// Generate both key factors at once
  /// \param recipient Recipient
  /// \returns Key factors
  [[nodiscard]] KeyFactors generateKeyFactors(const SkRecipient& recipient) const;

  /// Reshuffle and rekey encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption reshuffleRekey(
      const ElgamalEncryption& encryption,
      const KeyFactors& recipientKeyFactors) const;

  /// Rekey encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption rekey(
      const ElgamalEncryption& encryption,
      const ElgamalTranslationKey& recipientRekeyKeyFactor) const;

  /// Reshuffle encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption reshuffle(
      const ElgamalEncryption& encryption,
      const CurveScalar& recipientReshuffleKeyFactor) const;

  /// Reshuffle and rekey encryption with proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] std::pair<ElgamalEncryption, ReshuffleRekeyProof> certifiedReshuffleRekey(
      const ElgamalEncryption& encryption,
      const KeyFactors& recipientKeyFactors) const;

  /// Compute static public data necessary for verifying RSK proofs for a recipient
  /// \param recipientKeyFactors Recipient key factors
  /// \param masterPublicEncryptionKey Master public encryption key
  [[nodiscard]] ReshuffleRekeyVerifiers computeReshuffleRekeyVerifiers(
      const KeyFactors& recipientKeyFactors,
      const ElgamalPublicKey& masterPublicEncryptionKey) const;

  /// Generate an encryption key component.
  /// \details Components from all transcryptors should be multiplied to obtain an \c ElgamalPrivateKey to decrypt the data
  /// \param rekeyKeyFactor An encryption key factor
  /// \param masterPrivateEncryptionKeyShare Master private data/pseudonym encryption key share
  /// \returns Encryption key component
  [[nodiscard]] CurveScalar generateKeyComponent(
      const CurveScalar& rekeyKeyFactor,
      const CurveScalar& masterPrivateEncryptionKeyShare) const;

private:
  Keys keys_;
  EGCache* cache_; // Pointer instead of reference to allow copy/move operations on RskTranslator

  [[nodiscard]] CurveScalar generateKeyFactor(
      const KeyFactorSecret& keyFactorSecret,
      const RecipientBase& recipient) const;
};

} // namespace pep
