#include <pep/rsk/Proofs.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::create(
    const CurvePoint& A,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& x,
    CPRNG* rng) {
  auto nonce = rng == nullptr ? CurveScalar::Random()
                  : CurveScalar::Random<>(*rng);
  auto cb = nonce * CurvePoint::Base;
  auto cm = nonce * pre;
  auto challenge = computeChallenge(A, pre, post, cb, cm);
  pep::PublicCurveScalar s(nonce + (challenge * x));
  return ScalarMultProof(
    cb,
    cm,
    s
  );
}

void ScalarMultProof::verify(
    const CurvePoint& A,
    const CurvePoint& pre,
    const CurvePoint& post) const {
  pep::PublicCurveScalar challenge(computeChallenge(A, pre, post, mCB, mCM));
  if ((mS * CurvePoint::Base != challenge * A + mCB)
      || (mS * pre != challenge * post + mCM))
    throw InvalidProof();
}

void ScalarMultProof::ensurePacked() const {
  mCB.ensurePacked();
  mCM.ensurePacked();
}

CurveScalar ScalarMultProof::computeChallenge(
    const CurvePoint& A,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurvePoint& cb,
    const CurvePoint& cm) {
  std::string packed;
  packed.reserve(CurvePoint::PACKEDBYTES * 5);
  packed += A.pack();
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
    const CurvePoint& zB,
    const CurveScalar& zOverK,
    const CurvePoint& zOverKB,
    const CurveScalar& r,
    const CurvePoint& ry,
    const CurvePoint& rB,
    CPRNG* rng) {
  return RSKProof(
    ry,
    rB,
    ScalarMultProof::create(rB, pre.publicKey, ry, r, rng),
    ScalarMultProof::create(zOverKB, pre.b + rB, post.b, zOverK, rng),
    ScalarMultProof::create(zB, pre.c + ry, post.c, reshuffle, rng)
  );
}

RSKProof RSKProof::certifiedRSK(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const CurveScalar& rekey) {
  auto zOverK = reshuffle * rekey.invert();
  auto r = CurveScalar::Random();
  auto ry = r * in.publicKey;
  auto rB = r * CurvePoint::Base;

  out.b = zOverK * (in.b + rB);
  out.c = reshuffle * (in.c + ry);
  out.publicKey = rekey * in.publicKey;

  return RSKProof::create(
    in,
    out,
    reshuffle,
    reshuffle * CurvePoint::Base,
    zOverK,
    zOverK * CurvePoint::Base,
    r,
    ry,
    rB
  );
}

void RSKProof::ensurePacked() const {
  mRY.ensurePacked();
  mRB.ensurePacked();
  mRP.ensurePacked();
  mBP.ensurePacked();
  mCP.ensurePacked();
}

void RSKProof::verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const RSKVerifiers& verifiers) const {
  mRP.verify(mRB, pre.publicKey, mRY);
  mBP.verify(verifiers.mZOverKB, pre.b + mRB, post.b);
  mCP.verify(verifiers.mZB, pre.c + mRY, post.c);
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
  mZOverKB.ensureThreadSafe();
  mZB.ensureThreadSafe();
  mNewPublicKey.ensureThreadSafe();
}

}
