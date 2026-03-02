#include <pep/rsk/Proofs.hpp>

#include <pep/elgamal/CryptoAssert.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::Create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret,
    CPRNG* rng) {
  PEP_CryptoAssert(secretTimesBase == secret * CurvePoint::Base);
  PEP_CryptoAssert(post == secret * pre);
  auto nonce = rng == nullptr ? CurveScalar::Random()
                  : CurveScalar::Random<>(*rng);
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

ReshuffleRekeyProof ReshuffleRekeyProof::Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    CPRNG* rng) {
  PEP_CryptoAssert(reshufflePoint == reshuffle * CurvePoint::Base);
  PEP_CryptoAssert(reshuffleOverRekeyPoint == reshuffleOverRekey * CurvePoint::Base);
  return ReshuffleRekeyProof(
    ScalarMultProof::Create(reshuffleOverRekeyPoint, pre.b, post.b, reshuffleOverRekey, rng),
    ScalarMultProof::Create(reshufflePoint, pre.c, post.c, reshuffle, rng));
}

ReshuffleRekeyProof ReshuffleRekeyProof::CertifiedReshuffleRekey(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey,
    CPRNG* rng) {

  auto reshuffleOverRekey = reshuffle * rekey.invert();
  out = {
    reshuffleOverRekey * in.b,
    reshuffle * in.c,
    rekey * in.publicKey,
  };

  return Create(
    in,
    out,
    reshuffle,
    reshuffle * CurvePoint::Base,
    reshuffleOverRekey,
    reshuffleOverRekey * CurvePoint::Base,
    rng
  );
}

void ReshuffleRekeyProof::verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const ReshuffleRekeyVerifiers& verifiers) const {
  // Note: we assume that the factors in ReshuffleRekeyVerifiers are correctly related
  mReshuffleOverRekeyTimesBProof.verify(verifiers.mReshuffleOverRekeyPoint, pre.b, post.b);
  mReshuffleTimesCProof.verify(verifiers.mReshufflePoint, pre.c, post.c);
  if (post.publicKey != verifiers.mRekeyedPublicKey)
    throw InvalidProof();
}

RerandomizeProof RerandomizeProof::Create(
    const ElgamalPublicKey& publicKey,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    CPRNG* rng) {
  PEP_CryptoAssert(rerandomizePubKey == rerandomize * publicKey);
  PEP_CryptoAssert(rerandomizePoint == rerandomize * CurvePoint::Base);
  return RerandomizeProof(
    rerandomizePubKey,
    rerandomizePoint,
    ScalarMultProof::Create(rerandomizePoint, publicKey, rerandomizePubKey, rerandomize, rng));
}

RerandomizeProof RerandomizeProof::CertifiedRerandomize(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    CPRNG* rng) {
  auto rerandomize = rng ? CurveScalar::Random(*rng) : CurveScalar::Random();
  auto rerandomizePubKey = rerandomize * in.publicKey;
  auto rerandomizePoint = rerandomize * CurvePoint::Base;

  out = {
    in.b + rerandomizePoint,
    in.c + rerandomizePubKey,
    in.publicKey,
  };

  return Create(
    in.publicKey,
    rerandomize,
    rerandomizePubKey,
    rerandomizePoint,
    rng
  );
}

void RerandomizeProof::verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post) const {
  // Check the provided factors are related by the public key
  mRerandomizeTimesPubKeyProof.verify(mRerandomizePoint, pre.publicKey, mRerandomizePubKey);
  // Then use them to check that the ciphertext was correctly rerandomized
  // Comparisons don't need to be constant-time
  if (post.b != pre.b + mRerandomizePoint
      || post.c != pre.c + mRerandomizePubKey
      || post.publicKey != pre.publicKey) {
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
