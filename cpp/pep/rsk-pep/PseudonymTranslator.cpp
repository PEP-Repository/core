#include <pep/rsk-pep/PseudonymTranslator.hpp>

#include <pep/rsk-pep/KeyDomain.hpp>
#include <pep/utils/CollectionUtils.hpp>

using namespace pep;

// Public interface: doc comments in declaration

PseudonymTranslator::PseudonymTranslator(PseudonymTranslationKeys keys)
    : rsk_({
               .domain = static_cast<RskTranslator::KeyDomainType>(KeyDomain::Pseudonym),
               .reshuffle = keys.pseudonymizationKeyFactorSecret,
               .rekey = keys.encryptionKeyFactorSecret,
           }),
      masterPrivateEncryptionKeyShare_(keys.masterPrivateEncryptionKeyShare) {}

EncryptedLocalPseudonym PseudonymTranslator::translateStep(
    const EncryptedPseudonym& pseudonym,
    const Recipient& recipient
) const {
  return EncryptedLocalPseudonym(rsk_.rsk(pseudonym.getValidElgamalEncryption(), rsk_.generateKeyFactors(recipient)));
}

std::pair<EncryptedLocalPseudonym, RSKProof>
PseudonymTranslator::certifiedTranslateStep(
    const EncryptedPseudonym& pseudonym,
    const Recipient& recipient
) const {
  auto [encryption, proof] =
      rsk_.certifiedRsk(pseudonym.getValidElgamalEncryption(), rsk_.generateKeyFactors(recipient));
  return {EncryptedLocalPseudonym(encryption), proof};
}

RSKVerifiers PseudonymTranslator::computeTranslationProofVerifiers(
    const Recipient& recipient,
    const ElgamalPublicKey& masterPublicEncryptionKey
) const {
  return rsk_.computeRskProofVerifiers(
      rsk_.generateKeyFactors(recipient),
      masterPublicEncryptionKey);
}

void PseudonymTranslator::checkTranslationProof(
    const EncryptedPseudonym& preTranslate,
    const EncryptedLocalPseudonym& postTranslate,
    const RSKProof& proof,
    const RSKVerifiers& verifiers
) const {
  // Don't really need to check for nonzero pk, the proof is enough
  return proof.verify(
      preTranslate.getValidElgamalEncryption(),
      postTranslate.getValidElgamalEncryption(),
      verifiers);
}

CurveScalar PseudonymTranslator::generateKeyComponent(const RekeyRecipient& recipient) const {
  return rsk_.generateKeyComponent(
      rsk_.generateKeyFactor(recipient),
      CurveScalar(SpanToString(masterPrivateEncryptionKeyShare_.curveScalar())));
}
