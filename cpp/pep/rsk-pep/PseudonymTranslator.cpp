#include <pep/rsk-pep/PseudonymTranslator.hpp>

#include <pep/rsk-pep/KeyDomain.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/elgamal/CurveScalar.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp> // for ElgamalPublicKey
#include <pep/rsk-pep/PseudonymTranslationKeys.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/rsk/Proofs.hpp>
#include <pep/rsk/RskRecipient.hpp> // for RekeyRecipient

#include <utility>

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

std::pair<EncryptedLocalPseudonym, RskProof>
PseudonymTranslator::certifiedTranslateStep(
    const EncryptedPseudonym& pseudonym,
    const Recipient& recipient
) const {
  auto [encryption, proof] =
      rsk_.certifiedRsk(pseudonym.getValidElgamalEncryption(), rsk_.generateKeyFactors(recipient));
  return {EncryptedLocalPseudonym(encryption), proof};
}

ReshuffleRekeyVerifiers PseudonymTranslator::computeTranslationProofVerifiers(
    const Recipient& recipient,
    const ElgamalPublicKey& masterPublicEncryptionKey
) const {
  return rsk_.computeReshuffleRekeyVerifiers(
      rsk_.generateKeyFactors(recipient),
      masterPublicEncryptionKey);
}

ReshuffleRekeyVerifiersWithProof
PseudonymTranslator::computeCertifiedTranslationProofVerifiers(
    const Recipient& recipient,
    const ElgamalPublicKey& masterPublicEncryptionKey
) const {
  return rsk_.computeCertifiedReshuffleRekeyVerifiers(
      rsk_.generateKeyFactors(recipient),
      masterPublicEncryptionKey);
}

void PseudonymTranslator::checkTranslationProof(
    const EncryptedPseudonym& preTranslate,
    const EncryptedLocalPseudonym& postTranslate,
    const RskProof& proof,
    const ReshuffleRekeyVerifiers& verifiers
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
