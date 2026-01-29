#pragma once

#include <exception>
#include <vector>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/crypto/CPRNG.hpp>

namespace pep {

class InvalidProof : public std::exception {
  const char* what() const noexcept override {
    return "InvalidProof";
  }
};

// A compositional non-interactive zero-knowledge proof that
// CurvePoints (A, M, N) are in fact of the form (x B, M, x M).
// See https://docs.pages.pep.cs.ru.nl/private/ops/main/development/Design-logger/
// and §4 of "Lecture Notes Cryptographic Protocols" by Schoenmakers.
class ScalarMultProof {
  static CurveScalar computeChallenge(
      const CurvePoint& A,
      const CurvePoint& M,
      const CurvePoint& N,
      const CurvePoint& cb,
      const CurvePoint& cm);

 public:
  CurvePoint mCB;
  CurvePoint mCM;
  CurveScalar mS;

  ScalarMultProof() = default;
  ScalarMultProof(CurvePoint cb, CurvePoint cm, CurveScalar s)
    : mCB(cb), mCM(cm), mS(s) { }

  void ensurePacked() const; // See CurvePoint::ensurePacked()

  // Constructs a proof from A, M, N and x.
  //
  // Assumes A = x B and N = x M.
  static ScalarMultProof create(
    const CurvePoint& A,
    const CurvePoint& M,
    const CurvePoint& N,
    const CurveScalar& x,
    CPRNG* rng=nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const CurvePoint& A,
    const CurvePoint& M,
    const CurvePoint& N) const;
};

// Public data required to verify an RSKProof.
class RSKVerifiers {
 public:
  RSKVerifiers() = default;
  RSKVerifiers(
      CurvePoint zOverKB,
      CurvePoint zB,
      CurvePoint ky)
  : mZOverKB(zOverKB),
    mZB(zB),
    mKY(ky) { }
  static RSKVerifiers compute(
    const CurveScalar& z,
    const CurveScalar& k,
    const CurvePoint& y);

  CurvePoint mZOverKB;
  CurvePoint mZB;
  CurvePoint mKY;

  [[nodiscard]] auto operator<=>(const RSKVerifiers& right) const = default;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};



// A compositional non-interactive zero-knowledge proof that
// an ElgamalEncryption (b, c, y) has been (z,k)-RSKed to (b', c', y')
class RSKProof {
 public:
  RSKProof() = default;
  RSKProof(
      CurvePoint rY,
      CurvePoint rB,
      ScalarMultProof rp,
      ScalarMultProof bp,
      ScalarMultProof cp)
    : mRY(rY),
      mRB(rB),
      mRP(rp),
      mBP(bp),
      mCP(cp) { }

  CurvePoint mRY;
  CurvePoint mRB;

  ScalarMultProof mRP; // ScalarMultProof for (rb, y, ry)
  ScalarMultProof mBP; // ScalarMultProof for ((z/k)B, b + rb, b')
  ScalarMultProof mCP; // ScalarMultProof for (z B, c + ry, c')

  void ensurePacked() const; // See CurvePoint::ensurePacked()

  // Constructs a proof that post is the (z,k)-RSK of pre.
  //
  // Assumes zB = z B, zOverK = z/k, zOverKB = z/k B, ry = r y, rB = r B
  // and (of course) that post is the (z,k)-RSK of pre with random r.
  static RSKProof create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& z,
    const CurvePoint& zB,
    const CurveScalar& zOverK,
    const CurvePoint& zOverKB,
    const CurveScalar& r,
    const CurvePoint& ry,
    const CurvePoint& rB,
    CPRNG* rng=nullptr);

  // Stores the (z,k)-RSKed version of ElgamalEncryption in to out and
  // returns a zero-knowledge proof of correctness.
  //
  // XXX Add optimised version to EGCache
  static RSKProof certifiedRSK(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& z,
    const CurveScalar& k);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const RSKVerifiers& verifiers) const;
};

}
