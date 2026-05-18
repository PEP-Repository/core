#pragma once

#include <exception>
#include <vector>

#include <pep/elgamal/ElgamalEncryption.hpp>

namespace pep {

class InvalidProof : public std::exception {
  const char* what() const noexcept override {
    return "InvalidProof";
  }
};

// A compositional non-interactive zero-knowledge proof that
// CurvePoints (secretTimesBase, pre, post) are in fact of the form (secret*B, pre, secret*pre).
// See https://docs.pages.pep.cs.ru.nl/private/ops/main/technical_design/design-logger/
// and §4 of "Lecture Notes Cryptographic Protocols" by Schoenmakers.
class ScalarMultProof {
  static CurveScalar ComputeChallenge(
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
  // Assumes secretTimesBase = secret*B and post = secret*pre.
  static ScalarMultProof Create(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post,
    const CurveScalar& secret);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post) const;
};

// Public data required to verify an RskProof.
class ReshuffleRekeyVerifiers {
 public:
  ReshuffleRekeyVerifiers() = default;
  ReshuffleRekeyVerifiers(
      const CurvePoint& reshuffleOverRekeyPoint,
      const CurvePoint& reshufflePoint,
      const CurvePoint& rekeyedPublicKey)
  : mReshuffleOverRekeyPoint(reshuffleOverRekeyPoint),
    mReshufflePoint(reshufflePoint),
    mRekeyedPublicKey(rekeyedPublicKey) { }
  static ReshuffleRekeyVerifiers Compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const CurvePoint& publicKey);

  CurvePoint mReshuffleOverRekeyPoint;
  CurvePoint mReshufflePoint;
  CurvePoint mRekeyedPublicKey;

  [[nodiscard]] auto operator<=>(const ReshuffleRekeyVerifiers& right) const = default;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};



// A compositional non-interactive zero-knowledge proof that
// an ElgamalEncryption (b, c, publicKey) has been RSKed to (b', c', publicKey')
class RskProof {
 public:
  CurvePoint mRerandomizePubKey;
  CurvePoint mRerandomizePoint;
  ScalarMultProof mRerandomizeTimesPubKeyProof; // ScalarMultProof for rerandomize * publicKey
  ScalarMultProof mReshuffleOverRekeyTimesBProof; // ScalarMultProof for (reshuffle/rekey) * b
  ScalarMultProof mReshuffleTimesCProof; // ScalarMultProof for reshuffle * c

  RskProof() = default;
  RskProof(
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    const ScalarMultProof& rerandomizeTimesPubKeyProof,
    const ScalarMultProof& reshuffleOverRekeyTimesBProof,
    const ScalarMultProof& reshuffleTimesCProof)
  : mRerandomizePubKey(rerandomizePubKey),
    mRerandomizePoint(rerandomizePoint),
    mRerandomizeTimesPubKeyProof(rerandomizeTimesPubKeyProof),
    mReshuffleOverRekeyTimesBProof(reshuffleOverRekeyTimesBProof),
    mReshuffleTimesCProof(reshuffleTimesCProof) {}

  void ensurePacked() const {
    mRerandomizePubKey.ensurePacked();
    mRerandomizePoint.ensurePacked();
    mRerandomizeTimesPubKeyProof.ensurePacked();
    mReshuffleOverRekeyTimesBProof.ensurePacked();
    mReshuffleTimesCProof.ensurePacked();
  }

  // Constructs a proof that pre is RSKed to post.
  //
  // Assumes reshufflePoint = reshuffle*B, reshuffleOverRekey = reshuffle/rekey, reshuffleOverRekeyPoint = reshuffle/rekey*B,
  // rerandomizePubKey = rerandomize*publicKey, rerandomizePoint = rerandomize*B,
  // and (of course) that post is the RSKed version of pre.
  static RskProof Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint);

  // Stores the RSKed version of ElgamalEncryption in to out and
  // returns a zero-knowledge proof of correctness.
  //
  // XXX Add optimised version to EGCache
  static RskProof CertifiedRsk(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const ReshuffleRekeyVerifiers& verifiers) const;
};

}
