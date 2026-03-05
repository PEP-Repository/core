#include <pep/rsk/Proofs.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret,
    CPRNG* rng) {
  auto nonce = rng == nullptr ? CurveScalar::Random()
                  : CurveScalar::Random<>(*rng);
  auto cb = nonce * CurvePoint::Base;
  auto cm = nonce * pre;
  auto challenge = computeChallenge(secretTimesBase, pre, post, cb, cm);
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
  pep::PublicCurveScalar challenge(computeChallenge(secretTimesBase, pre, post, mCB, mCM));
  if ((mS * CurvePoint::Base != challenge * secretTimesBase + mCB)
      || (mS * pre != challenge * post + mCM))
    throw InvalidProof();
}

void ScalarMultProof::ensurePacked() const {
  mCB.ensurePacked();
  mCM.ensurePacked();
}

CurveScalar ScalarMultProof::computeChallenge(
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

RSKProof RSKProof::create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    CPRNG* rng) {
  return RSKProof(
    rerandomizePubKey,
    rerandomizePoint,
    ScalarMultProof::create(rerandomizePoint, pre.publicKey, rerandomizePubKey, rerandomize, rng),
    ScalarMultProof::create(reshuffleOverRekeyPoint, pre.b + rerandomizePoint, post.b, reshuffleOverRekey, rng),
    ScalarMultProof::create(reshufflePoint, pre.c + rerandomizePubKey, post.c, reshuffle, rng)
  );
}

RSKProof RSKProof::certifiedRSK(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const CurveScalar& rekey) {
  auto reshuffleOverRekey = reshuffle * rekey.invert();
  auto rerandomize = CurveScalar::Random();
  auto rerandomizePubKey = rerandomize * in.publicKey;
  auto rerandomizePoint = rerandomize * CurvePoint::Base;

  out.b = reshuffleOverRekey * (in.b + rerandomizePoint);
  out.c = reshuffle * (in.c + rerandomizePubKey);
  out.publicKey = rekey * in.publicKey;

  return RSKProof::create(
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

void RSKProof::ensurePacked() const {
  mRerandomizePubKey.ensurePacked();
  mRerandomizePoint.ensurePacked();
  mRP.ensurePacked();
  mBP.ensurePacked();
  mCP.ensurePacked();
}

void RSKProof::verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const RSKVerifiers& verifiers) const {
  mRP.verify(mRerandomizePoint, pre.publicKey, mRerandomizePubKey);
  mBP.verify(verifiers.mReshuffleOverRekeyPoint, pre.b + mRerandomizePoint, post.b);
  mCP.verify(verifiers.mReshufflePoint, pre.c + mRerandomizePubKey, post.c);
  if (post.publicKey != verifiers.mNewPublicKey)
    throw InvalidProof();
}

RSKVerifiers RSKVerifiers::compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const CurvePoint& publicKey) {
  return RSKVerifiers(
    reshuffle * rekey.invert() * CurvePoint::Base,
    reshuffle * CurvePoint::Base,
    rekey * publicKey
  );
}

void RSKVerifiers::ensureThreadSafe() const {
  mReshuffleOverRekeyPoint.ensureThreadSafe();
  mReshufflePoint.ensureThreadSafe();
  mNewPublicKey.ensureThreadSafe();
}

}
