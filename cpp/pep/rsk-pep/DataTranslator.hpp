#pragma once

#include <pep/crypto/CPRNG.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/rsk-pep/DataTranslationKeys.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/rsk/RskTranslator.hpp>

#include <string_view>

namespace pep {

class DataTranslator {
  RskTranslator rsk_;
  /// \copydoc DataTranslationKeys::masterPrivateEncryptionKeyShare
  MasterPrivateKeyShare masterPrivateEncryptionKeyShare_;

  enum class BlindMode { Blind, Unblind };

  // Private interface: doc comments in implementation

  [[nodiscard]] CurveScalar generateBlindingKey(
      BlindMode blindMode,
      std::string_view blindAddData,
      bool invertBlindKey) const;

public:
  using Recipient = RekeyRecipient;

  explicit DataTranslator(DataTranslationKeys keys);

  /// Blind encrypted data (only for AM)
  /// \param unblinded Unblinded encrypted data
  /// \param blindAddData See Metadata::computeKeyBlindingAdditionalData
  /// \param invertBlindKey Invert the blinding key instead of the unblinding key? (New behavior)
  /// \returns Blinded encrypted data
  /// \throws std::logic_error if the blinding key secret is not set in the keys
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption blind(
      const ElgamalEncryption& unblinded,
      std::string_view blindAddData,
      bool invertBlindKey) const;

  /// Unblind encrypted data and do a translation step
  /// \param blinded Blinded encrypted data
  /// \param blindingAddData See Metadata::computeKeyBlindingAdditionalData
  /// \param invertBlindKey Invert the blinding key instead of the unblinding key? (New behavior)
  /// \param recipient Recipient
  /// \returns Unblinded and translated encrypted data
  /// \throws std::logic_error if the blinding key secret is not set in the keys
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption unblindAndTranslate(
      const ElgamalEncryption& blinded,
      std::string_view blindingAddData,
      bool invertBlindKey,
      const Recipient& recipient) const;

  /// Do a translation step without unblinding
  /// \param encrypted Encrypted data
  /// \param recipient Recipient
  /// \returns (Partially) translated encrypted data
  /// \throws std::invalid_argument for invalid encryption
  [[nodiscard]] ElgamalEncryption translateStep(
      const ElgamalEncryption& encrypted,
      const Recipient& recipient) const;

  /// Generate a data encryption key component for \p recipient
  /// \returns Data encryption key component
  [[nodiscard]] CurveScalar generateKeyComponent(const Recipient& recipient) const;
};

} // pep
