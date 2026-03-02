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
// CurvePoints (secretTimesBase, pre, post) are in fact of the form (secret B, pre, secret pre).
// See https://docs.pages.pep.cs.ru.nl/private/ops/main/technical_design/design-logger/
// and §4 of "Lecture Notes Cryptographic Protocols" by Schoenmakers.
class ScalarMultProof {
  static CurveScalar computeChallenge(
      const CurvePoint& secretTimesBase,
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

  // Constructs a proof from secretTimesBase, pre, post and secret.
  //
  // Assumes secretTimesBase = secret B and post = secret pre.
  static ScalarMultProof create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret,
    CPRNG* rng=nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post) const;
};

// Public data required to verify an RSKProof.
class RSKVerifiers {
 public:
  RSKVerifiers() = default;
  RSKVerifiers(
      const CurvePoint& reshuffleOverRekeyPoint,
      const CurvePoint& reshufflePoint,
      const CurvePoint& newPublicKey)
  : mReshuffleOverRekeyPoint(reshuffleOverRekeyPoint),
    mReshufflePoint(reshufflePoint),
    mNewPublicKey(newPublicKey) { }
  static RSKVerifiers compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const CurvePoint& publicKey);

  CurvePoint mReshuffleOverRekeyPoint;
  CurvePoint mReshufflePoint;
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
      CurvePoint rerandomizePubKey,
      CurvePoint rerandomizePoint,
      ScalarMultProof rp,
      ScalarMultProof bp,
      ScalarMultProof cp)
    : mRerandomizePubKey(rerandomizePubKey),
      mRerandomizePoint(rerandomizePoint),
      mRP(rp),
      mBP(bp),
      mCP(cp) { }

  CurvePoint mRerandomizePubKey;
  CurvePoint mRerandomizePoint;

  ScalarMultProof mRP; // ScalarMultProof for (rerandomize B, publicKey, rerandomizePubKey)
  ScalarMultProof mBP; // ScalarMultProof for ((reshuffle/rekey)B, b + rerandomize B, b')
  ScalarMultProof mCP; // ScalarMultProof for (reshuffle B, c + rerandomizePubKey, c')

  void ensurePacked() const; // See CurvePoint::ensurePacked()

  // Constructs a proof that post is the (reshuffle,rekey)-RSK of pre.
  //
  // Assumes reshufflePoint = reshuffle B, reshuffleOverRekey = reshuffle/rekey, reshuffleOverRekeyPoint = reshuffle/rekey B, rerandomizePubKey = rerandomize publicKey, rerandomizePoint = rerandomize B
  // and (of course) that post is the (reshuffle,rekey)-RSK of pre with random rerandomize.
  static RSKProof create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
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
