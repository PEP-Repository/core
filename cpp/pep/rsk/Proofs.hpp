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
    const CurveScalar& secret,
    CPRNG* rng=nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const CurvePoint& secretTimesBase,
    const CurvePoint& pre,
    const CurvePoint& post) const;
};

// Public data required to verify a ReshuffleRekeyProof.
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
// an ElgamalEncryption (b, c, publicKey) has been reshuffled & rekeyed to (b', c', publicKey')
class ReshuffleRekeyProof {
 public:
  ScalarMultProof mReshuffleOverRekeyTimesBProof; // ScalarMultProof for (reshuffle/rekey) * b
  ScalarMultProof mReshuffleTimesCProof; // ScalarMultProof for reshuffle * c

  ReshuffleRekeyProof() = default;
  ReshuffleRekeyProof(
    const ScalarMultProof& reshuffleOverRekeyTimesBProof,
    const ScalarMultProof& reshuffleTimesCProof)
  : mReshuffleOverRekeyTimesBProof(reshuffleOverRekeyTimesBProof),
    mReshuffleTimesCProof(reshuffleTimesCProof) {}

  void ensurePacked() const {
    mReshuffleOverRekeyTimesBProof.ensurePacked();
    mReshuffleTimesCProof.ensurePacked();
  }

  // Constructs a proof that pre is reshuffled & rekeyed to post.
  //
  // Assumes reshufflePoint = reshuffle*B, reshuffleOverRekey = reshuffle/rekey, reshuffleOverRekeyPoint = reshuffle/rekey*B
  // and (of course) that post is the reshuffled & rekeyed version of pre.
  static ReshuffleRekeyProof Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshufflePoint,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyPoint,
    CPRNG* rng = nullptr);

  // Stores the reshuffled & rekeyed version of ElgamalEncryption in to out and
  // returns a zero-knowledge proof of correctness.
  //
  // XXX Add optimised version to EGCache
  static ReshuffleRekeyProof CertifiedReshuffleRekey(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    const CurveScalar& reshuffle,
    const ElgamalTranslationKey& rekey,
    CPRNG* rng = nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const ReshuffleRekeyVerifiers& verifiers) const;
};

/// Proof that one encryption is the rerandomized version of another
class RerandomizeProof {
 public:
  CurvePoint mRerandomizePubKey;
  CurvePoint mRerandomizePoint;
  ScalarMultProof mRerandomizeTimesPubKeyProof; // ScalarMultProof for rerandomize * publicKey

  RerandomizeProof() = default;
  RerandomizeProof(
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    const ScalarMultProof& rerandomizeTimesPubKeyProof)
  : mRerandomizePubKey(rerandomizePubKey),
    mRerandomizePoint(rerandomizePoint),
    mRerandomizeTimesPubKeyProof(rerandomizeTimesPubKeyProof) {}

  void ensurePacked() const {
    mRerandomizePubKey.ensurePacked();
    mRerandomizePoint.ensurePacked();
    mRerandomizeTimesPubKeyProof.ensurePacked();
  }

  // Assumes rerandomizePubKey = rerandomize*publicKey, rerandomizePoint = rerandomize*B
  static RerandomizeProof Create(
    const ElgamalPublicKey& publicKey,
    const CurveScalar& rerandomize,
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    CPRNG* rng = nullptr);

  // XXX Add optimised version to EGCache
  static RerandomizeProof CertifiedRerandomize(
    const ElgamalEncryption& in,
    ElgamalEncryption& out,
    CPRNG* rng = nullptr);

  // Checks the proof. Throws InvalidProof if the proof is invalid.
  void verify(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post) const;
};

}
