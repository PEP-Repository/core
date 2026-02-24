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
// CurvePoints (A, pre, post) are in fact of the form (x B, pre, x pre).
// See https://docs.pages.pep.cs.ru.nl/private/ops/main/technical_design/design-logger/
// and §4 of "Lecture Notes Cryptographic Protocols" by Schoenmakers.
class ScalarMultProof {
  static CurveScalar computeChallenge(
      const CurvePoint& A,
      const CurvePoint& pre,
      const CurvePoint& post,
      const CurvePoint& cb,
      const CurvePoint& cm);

 public:
  CurvePoint mCB;
  CurvePoint mCM;
  PublicCurveScalar mS;

  ScalarMultProof() = default;
  ScalarMultProof(const CurvePoint& cb, const CurvePoint& cm, const PublicCurveScalar& s)
    : mCB(cb), mCM(cm), mS(s) { }

  void ensurePacked() const; // See CurvePoint::ensurePacked()

  // Constructs a proof from A, pre, post and x.
  //
  // Assumes A = x B and post = x pre.
  static ScalarMultProof create(
    const CurvePoint& A,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& x,
    CPRNG* rng=nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const CurvePoint& A,
    const CurvePoint& pre,
    const CurvePoint& post) const;
};

// Public data required to verify an RSKProof.
class RSKVerifiers {
 public:
  RSKVerifiers() = default;
  RSKVerifiers(
      const CurvePoint& zOverKB,
      const CurvePoint& zB,
      const CurvePoint& newPublicKey)
  : mZOverKB(zOverKB),
    mZB(zB),
    mNewPublicKey(newPublicKey) { }
  static RSKVerifiers compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const CurvePoint& publicKey);

  CurvePoint mZOverKB;
  CurvePoint mZB;
  CurvePoint mNewPublicKey;

  [[nodiscard]] auto operator<=>(const RSKVerifiers& right) const = default;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};



// A compositional non-interactive zero-knowledge proof that
// an ElgamalEncryption (b, c, publicKey) has been (reshuffle,rekey)-RSKed to (b', c', publicKey')
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

  ScalarMultProof mRP; // ScalarMultProof for (rb, publicKey, ry)
  ScalarMultProof mBP; // ScalarMultProof for ((reshuffle/rekey)B, b + rb, b')
  ScalarMultProof mCP; // ScalarMultProof for (reshuffle B, c + ry, c')

  void ensurePacked() const; // See CurvePoint::ensurePacked()

  // Constructs a proof that post is the (reshuffle,rekey)-RSK of pre.
  //
  // Assumes zB = reshuffle B, zOverK = reshuffle/rekey, zOverKB = reshuffle/rekey B, ry = r publicKey, rB = r B
  // and (of course) that post is the (reshuffle,rekey)-RSK of pre with random r.
  static RSKProof create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& zB,
    const CurveScalar& zOverK,
    const CurvePoint& zOverKB,
    const CurveScalar& r,
    const CurvePoint& ry,
    const CurvePoint& rB,
    CPRNG* rng=nullptr);

  // Stores the (reshuffle,rekey)-RSKed version of ElgamalEncryption in to out and
  // returns a zero-knowledge proof of correctness.
  //
  // XXX Add optimised version to EGCache
  static RSKProof certifiedRSK(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const CurveScalar& rekey);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const RSKVerifiers& verifiers) const;
};

}
