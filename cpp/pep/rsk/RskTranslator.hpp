#pragma once

#include <pep/crypto/CPRNG.hpp>
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
  };

  struct KeyFactors {
    /// I.e. pseudonymization / blinding
    CurveScalar reshuffle;
    /// Encryption
    ElgamalTranslationKey rekey;
  };

  explicit RskTranslator(Keys keys);

  const Keys& keys() const { return keys_; }

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

  /// Rerandomize, reshuffle, and rekey encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption rsk(
      const ElgamalEncryption& encryption,
      const KeyFactors& recipientKeyFactors) const;

  /// Rerandomize and rekey encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption rk(
      const ElgamalEncryption& encryption,
      const ElgamalTranslationKey& recipientRekeyKeyFactor) const;

  /// Rerandomize and reshuffle encryption without proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption rs(
      const ElgamalEncryption& encryption,
      const CurveScalar& recipientReshuffleKeyFactor) const;

  /// Rerandomize, reshuffle, and rekey encryption with proof
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] std::pair<ElgamalEncryption, RSKProof> certifiedRsk(
      const ElgamalEncryption& encryption,
      const KeyFactors& recipientKeyFactors) const;

  /// Compute static public data necessary for verifying RSK proofs for a recipient
  /// \param recipientKeyFactors Recipient key factors
  /// \param masterPublicEncryptionKey Master public encryption key
  [[nodiscard]] RSKVerifiers computeRskProofVerifiers(
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

  // This is a pointer so that RskTranslator remains movable (mutex is not)
  std::unique_ptr<CPRNG> rng_; //TODO Is this actually more performant than RandomBytes? As it currently uses locks internally (see note)

  [[nodiscard]] CurveScalar generateKeyFactor(
      const KeyFactorSecret& keyFactorSecret,
      const RecipientBase& recipient) const;
};

} // namespace pep
