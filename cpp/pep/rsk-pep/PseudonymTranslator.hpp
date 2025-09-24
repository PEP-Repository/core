#pragma once

#include <pep/crypto/CPRNG.hpp>
#include <pep/rsk-pep/PseudonymTranslationKeys.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/EGCache.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/rsk/RskTranslator.hpp>

#include <utility>

namespace pep {

class PseudonymTranslator {
  RskTranslator rsk_;
  /// \copydoc PseudonymTranslationKeys::masterPrivateEncryptionKeyShare
  MasterPrivateKeyShare masterPrivateEncryptionKeyShare_;

public:
  using Recipient = SkRecipient;

  explicit PseudonymTranslator(PseudonymTranslationKeys keys);

  /// Do a translation step without proof of \p pseudonym for \p recipient
  /// \param pseudonym Polymorphic or partially translated pseudonym
  /// \param recipient Recipient
  /// \returns (Partially) translated pseudonym
  /// \throws std::invalid_argument for invalid pseudonym
  [[nodiscard]] EncryptedLocalPseudonym translateStep(
      const EncryptedPseudonym& pseudonym,
      const Recipient& recipient) const;

  /// Do a translation step with proof of \p pseudonym for \p recipient
  /// \param pseudonym Polymorphic or partially translated pseudonym
  /// \param recipient Recipient
  /// \returns (Partially) translated pseudonym and proof
  /// \throws std::invalid_argument for invalid pseudonym
  [[nodiscard]] std::pair<EncryptedLocalPseudonym, RSKProof> certifiedTranslateStep(
      const EncryptedPseudonym& pseudonym,
      const Recipient& recipient) const;

  /// Compute static public data necessary for verifying translation proofs with recipient \p recipient
  /// \param recipient Recipient
  /// \param masterPublicEncryptionKey Master public pseudonym encryption key
  /// \throws std::invalid_argument for invalid pseudonym
  /// \note This only works if we do the first translation step
  [[nodiscard]] RSKVerifiers computeTranslationProofVerifiers(
      const Recipient& recipient,
      const ElgamalPublicKey& masterPublicEncryptionKey) const;

  /// Check translation proof correctness
  /// \param preTranslate Polymorphic or partially translated pseudonym from before translation step
  /// \param postTranslate Pseudonym after translation step
  /// \param proof Translation proof
  /// \param verifiers Static public data necessary for verifying translation proofs for the correct recipient,
  ///   generated using \c computeTranslationProofVerifiers by other party
  /// \throws std::invalid_argument for invalid pseudonym
  /// \throws InvalidProof for invalid proof
  void checkTranslationProof(
      const EncryptedPseudonym& preTranslate,
      const EncryptedLocalPseudonym& postTranslate,
      const RSKProof& proof, const RSKVerifiers& verifiers) const;

  /// Generate a pseudonym encryption key component for \p recipient
  /// \returns Pseudonym encryption key component
  [[nodiscard]] CurveScalar generateKeyComponent(const RekeyRecipient& recipient) const;
};

} // namespace pep
