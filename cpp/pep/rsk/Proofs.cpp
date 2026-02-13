#include <pep/rsk/Proofs.hpp>

namespace pep {

ScalarMultProof ScalarMultProof::create(
    const CurvePoint& A,
    const CurvePoint& M,
    const CurvePoint& N,
    const CurveScalar& x,
    CPRNG* rng) {
  auto nonce = rng == nullptr ? CurveScalar::Random()
                  : CurveScalar::Random<>(*rng);
  auto cb = CurvePoint::BaseMult(nonce);
  auto cm = M * nonce;
  auto challenge = computeChallenge(A, M, N, cb, cm);
  return ScalarMultProof(
    cb,
    cm,
    nonce + (challenge * x)
  );
}

void ScalarMultProof::verify(
    const CurvePoint& A,
    const CurvePoint& M,
    const CurvePoint& N) const {
  auto challenge = computeChallenge(A, M, N, mCB, mCM);
  if ((CurvePoint::PublicBaseMult(mS) != A.publicMult(challenge) + mCB)
      || (M.publicMult(mS) != N.publicMult(challenge) + mCM))
    throw InvalidProof();
}

void ScalarMultProof::ensurePacked() const {
  mCB.ensurePacked();
  mCM.ensurePacked();
}

CurveScalar ScalarMultProof::computeChallenge(
    const CurvePoint& A,
    const CurvePoint& M,
    const CurvePoint& N,
    const CurvePoint& cb,
    const CurvePoint& cm) {
  std::string packed;
  packed.reserve(CurvePoint::PACKEDBYTES * 5);
  packed += A.pack();
  packed += M.pack();
  packed += N.pack();
  packed += cb.pack();
  packed += cm.pack();
  return CurveScalar::ShortHash(packed);
}

RSKProof RSKProof::create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& z,
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
    ScalarMultProof::create(rB, pre.y, ry, r, rng),
    ScalarMultProof::create(zOverKB, pre.b + rB, post.b, zOverK, rng),
    ScalarMultProof::create(zB, pre.c + ry, post.c, z, rng)
  );
}

RSKProof RSKProof::certifiedRSK(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& z,
    const CurveScalar& k) {
  auto zOverK = z * k.invert();
  auto r = CurveScalar::Random();
  auto ry = in.y * r;
  auto rB = CurvePoint::BaseMult(r);

  out.b = (in.b + rB) * zOverK;
  out.c = (in.c + ry) * z;
  out.y = in.y * k;

  return RSKProof::create(
    in,
    out,
    z,
    CurvePoint::BaseMult(z),
    zOverK,
    CurvePoint::BaseMult(zOverK),
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
  mRP.verify(mRB, pre.y, mRY);
  mBP.verify(verifiers.mZOverKB, pre.b + mRB, post.b);
  mCP.verify(verifiers.mZB, pre.c + mRY, post.c);
  if (post.y != verifiers.mKY)
    throw InvalidProof();
}

RSKVerifiers RSKVerifiers::compute(
    const CurveScalar& z,
    const CurveScalar& k,
    const CurvePoint& y) {
  return RSKVerifiers(
    CurvePoint::BaseMult(z * k.invert()),
    CurvePoint::BaseMult(z),
    y * k
  );
}

void RSKVerifiers::ensureThreadSafe() const {
  mZOverKB.ensureThreadSafe();
  mZB.ensureThreadSafe();
  mKY.ensureThreadSafe();
}

}
