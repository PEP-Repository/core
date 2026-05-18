#include <pep/rsk/Proofs.hpp>

#include <pep/elgamal/CryptoAssert.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::Create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret) {
  PEP_CryptoAssert(secretTimesBase == secret * CurvePoint::Base);
  PEP_CryptoAssert(post == secret * pre);
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
  pep::PublicCurveScalar challenge(ComputeChallenge(secretTimesBase, pre, post, mCB, mCM));
  if ((mS * CurvePoint::Base != challenge * secretTimesBase + mCB)
      || (mS * pre != challenge * post + mCM))
    throw InvalidProof();
}

void ScalarMultProof::ensurePacked() const {
  mCB.ensurePacked();
  mCM.ensurePacked();
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

RskProof RskProof::Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint) {
  PEP_CryptoAssert(reshufflePoint == reshuffle * CurvePoint::Base);
  PEP_CryptoAssert(reshuffleOverRekeyPoint == reshuffleOverRekey * CurvePoint::Base);
  return RskProof(
    rerandomizePubKey,
    rerandomizePoint,
    ScalarMultProof::Create(rerandomizePoint, pre.publicKey, rerandomizePubKey, rerandomize),
    ScalarMultProof::Create(reshuffleOverRekeyPoint, pre.b + rerandomizePoint, post.b, reshuffleOverRekey),
    ScalarMultProof::Create(reshufflePoint, pre.c + rerandomizePubKey, post.c, reshuffle));
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
  mRerandomizeTimesPubKeyProof.verify(mRerandomizePoint, pre.publicKey, mRerandomizePubKey);

  // Note: we assume that the factors in ReshuffleRekeyVerifiers are correctly related
  mReshuffleOverRekeyTimesBProof.verify(verifiers.mReshuffleOverRekeyPoint, pre.b + mRerandomizePoint, post.b);
  mReshuffleTimesCProof.verify(verifiers.mReshufflePoint, pre.c + mRerandomizePubKey, post.c);
  if (post.publicKey != verifiers.mRekeyedPublicKey) {
    throw InvalidProof();
  }
}

ReshuffleRekeyVerifiers ReshuffleRekeyVerifiers::Compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const CurvePoint& publicKey) {
  return ReshuffleRekeyVerifiers(
    reshuffle * rekey.invert() * CurvePoint::Base,
    reshuffle * CurvePoint::Base,
    rekey * publicKey
  );
}

void ReshuffleRekeyVerifiers::ensureThreadSafe() const {
  mReshuffleOverRekeyPoint.ensureThreadSafe();
  mReshufflePoint.ensureThreadSafe();
  mRekeyedPublicKey.ensureThreadSafe();
}

}
