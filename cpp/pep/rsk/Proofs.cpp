#include <pep/rsk/Proofs.hpp>

#include <pep/elgamal/CryptoAssert.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::Create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret) {
  PEP_CRYPTO_ASSERT(secretTimesBase == secret * CurvePoint::Base);
  PEP_CRYPTO_ASSERT(post == secret * pre);
  auto nonce = CurveScalar::Random();
  auto cb = nonce * CurvePoint::Base;
  auto cm = nonce * pre;
  auto challenge = ComputeChallenge(secretTimesBase, pre, post, cb, cm);
  pep::PublicCurveScalar s(nonce + (challenge * secret));
  return ScalarMultProof(
    cb,
    cm,
    s
  );
}

void ScalarMultProof::verify(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post) const {
  pep::PublicCurveScalar challenge(ComputeChallenge(secretTimesBase, pre, post, cB_, cM_));
  if ((mS_ * CurvePoint::Base != challenge * secretTimesBase + cB_)
      || (mS_ * pre != challenge * post + cM_))
    throw InvalidProof();
}

void ScalarMultProof::ensurePacked() const {
  cB_.ensurePacked();
  cM_.ensurePacked();
}

CurveScalar ScalarMultProof::ComputeChallenge(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurvePoint& cb,
    const CurvePoint& cm) {
  std::string packed;
  packed.reserve(CurvePoint::PACKEDBYTES * 5);
  packed += secretTimesBase.pack();
  packed += pre.pack();
  packed += post.pack();
  packed += cb.pack();
  packed += cm.pack();
  return CurveScalar::ShortHash(packed);
}

InverseProof InverseProof::Create(
    const CurvePoint& secretInversePoint,
    const CurvePoint& secretAsPoint,
    const CurveScalar& secretInverse) {
  // x^{-1} * X = B
  return InverseProof(ScalarMultProof::Create(secretInversePoint, secretAsPoint, CurvePoint::Base, secretInverse));
}

void InverseProof::verify(
    const CurvePoint& secretInversePoint,
    const CurvePoint& secretAsPoint) const {
  return secretInverseTimesPointProof_.verify(secretInversePoint, secretAsPoint, CurvePoint::Base);
}

RskProof RskProof::Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshuffleCommitment,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyCommitment,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint) {
  PEP_CRYPTO_ASSERT(reshuffleCommitment == reshuffle * CurvePoint::Base);
  PEP_CRYPTO_ASSERT(reshuffleOverRekeyCommitment == reshuffleOverRekey * CurvePoint::Base);
  return RskProof(
    rerandomizePubKey,
    rerandomizePoint,
    ScalarMultProof::Create(rerandomizePoint, pre.publicKey, rerandomizePubKey, rerandomize),
    ScalarMultProof::Create(reshuffleOverRekeyCommitment, pre.b + rerandomizePoint, post.b, reshuffleOverRekey),
    ScalarMultProof::Create(reshuffleCommitment, pre.c + rerandomizePubKey, post.c, reshuffle));
}

RskProof RskProof::CertifiedRsk(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey) {

  auto reshuffleOverRekey = reshuffle * rekey.invert();
  auto rerandomize = CurveScalar::Random();
  auto rerandomizePubKey = rerandomize * in.publicKey;
  auto rerandomizePoint = rerandomize * CurvePoint::Base;
  out = {
    reshuffleOverRekey * (in.b + rerandomizePoint),
    reshuffle * (in.c + rerandomizePubKey),
    rekey * in.publicKey,
  };

  return Create(
    in,
    out,
    reshuffle,
    reshuffle * CurvePoint::Base,
    reshuffleOverRekey,
    reshuffleOverRekey * CurvePoint::Base,
    rerandomize,
    rerandomizePubKey,
    rerandomizePoint
  );
}

void RskProof::verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const ReshuffleRekeyVerifiers& verifiers) const {
  // Check the provided factors are related by the public key
  rerandomizeTimesPubKeyProof_.verify(rerandomizePoint_, pre.publicKey, rerandomizePubKey_);

  // Note: we assume that the factors in ReshuffleRekeyVerifiers are correctly related
  reshuffleOverRekeyTimesBProof_.verify(verifiers.reshuffleOverRekeyCommitment_, pre.b + rerandomizePoint_, post.b);
  reshuffleTimesCProof_.verify(verifiers.reshuffleCommitment_, pre.c + rerandomizePubKey_, post.c);
  if (post.publicKey != verifiers.rekeyedPublicKey_) {
    throw InvalidProof();
  }
}

ReshuffleRekeyVerifiers ReshuffleRekeyVerifiers::Compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const ElgamalPublicKey& globalKey) {
  CurvePoint reshuffleCommitment = reshuffle * CurvePoint::Base;
  return ReshuffleRekeyVerifiers(
    reshuffleCommitment,
    rekey * CurvePoint::Base,
    rekey.invert() * reshuffleCommitment,
    rekey * globalKey
  );
}

void ReshuffleRekeyVerifiers::ensureThreadSafe() const {
  reshuffleOverRekeyCommitment_.ensureThreadSafe();
  reshuffleCommitment_.ensureThreadSafe();
  rekeyedPublicKey_.ensureThreadSafe();
}

void ReshuffleRekeyVerifiersProof::verify(
    const ReshuffleRekeyVerifiers& verifiers,
    const ElgamalPublicKey& globalKey) const {
  // Check rekeyCommitment & rekeyInversePoint are related
  rekeyInverseProof_.verify(rekeyInversePoint_, verifiers.rekeyCommitment_);
  // Check log(reshuffleCommitment) * rekeyInversePoint = reshuffleOverRekeyCommitment
  reshuffleTimesRekeyInverseProof_.verify(verifiers.reshuffleCommitment_, rekeyInversePoint_, verifiers.reshuffleOverRekeyCommitment_);
  // Check public key
  rekeyTimesPublicKeyProof_.verify(verifiers.rekeyCommitment_, globalKey, verifiers.rekeyedPublicKey_);
}

ReshuffleRekeyVerifiersWithProof
ReshuffleRekeyVerifiersProof::ComputeCertified(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const ElgamalPublicKey& globalKey) {
  CurveScalar rekeyInverse = rekey.invert();
  CurvePoint rekeyInversePoint = rekeyInverse * CurvePoint::Base;
  CurvePoint reshuffleCommitment = reshuffle * CurvePoint::Base; //TODO only calculate when not yet known
  ReshuffleRekeyVerifiers verifiers(
    reshuffleCommitment,
    rekey * CurvePoint::Base,
    rekeyInverse * reshuffleCommitment,
    rekey * globalKey
  );
  ReshuffleRekeyVerifiersProof proof(
    rekeyInversePoint,
    InverseProof::Create(rekeyInversePoint, verifiers.rekeyCommitment_, rekeyInverse),
    ScalarMultProof::Create(reshuffleCommitment, rekeyInversePoint, verifiers.reshuffleOverRekeyCommitment_, reshuffle),
    ScalarMultProof::Create(verifiers.rekeyCommitment_, globalKey, verifiers.rekeyedPublicKey_, rekey));
  return {verifiers, proof};
}

}
