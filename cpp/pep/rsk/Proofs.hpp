#pragma once

#include <exception>
#include <vector>

#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/serialization/Serializer.hpp>

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

  CurvePoint cB_;
  CurvePoint cM_;
  PublicCurveScalar mS_;

  friend class Serializer<ScalarMultProof>;

public:
  ScalarMultProof() = default;
  ScalarMultProof(const CurvePoint& cb, const CurvePoint& cm, const PublicCurveScalar& s)
    : cB_(cb), cM_(cm), mS_(s) { }

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

/// Proof that a point X^{-1} is in the form of x^{-1}B, given X = xB
class InverseProof {
public:
  ScalarMultProof secretInverseTimesPointProof_;

  InverseProof() = default;
  explicit InverseProof(const ScalarMultProof& secretInverseTimesPointProof)
    : secretInverseTimesPointProof_(secretInverseTimesPointProof) {}

  static InverseProof Create(
    const CurvePoint& secretInversePoint,
    const CurvePoint& secretAsPoint,
    const CurveScalar& secretInverse);

  void verify(
    const CurvePoint& secretInversePoint,
    const CurvePoint& secretAsPoint) const;
};

// Public data required to verify an RskProof.
class ReshuffleRekeyVerifiers {
 public:
  ReshuffleRekeyVerifiers() = default;
  ReshuffleRekeyVerifiers(
      const CurvePoint& reshuffleCommitment,
      const CurvePoint& rekeyCommitment,
      const CurvePoint& reshuffleOverRekeyCommitment,
      const ElgamalPublicKey& rekeyedPublicKey)
  : reshuffleCommitment(reshuffleCommitment),
    rekeyCommitment(rekeyCommitment),
    reshuffleOverRekeyCommitment(reshuffleOverRekeyCommitment),
    rekeyedPublicKey(rekeyedPublicKey) { }
  static ReshuffleRekeyVerifiers Compute(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const ElgamalPublicKey& globalKey);

  CurvePoint reshuffleCommitment;
  CurvePoint rekeyCommitment;
  CurvePoint reshuffleOverRekeyCommitment;
  ElgamalPublicKey rekeyedPublicKey;

  [[nodiscard]] auto operator<=>(const ReshuffleRekeyVerifiers& right) const = default;

  void ensureThreadSafe() const; // See CurvePoint::ensureThreadSafe()
};

using ReshuffleRekeyVerifiersWithProof = std::pair<ReshuffleRekeyVerifiers, class ReshuffleRekeyVerifiersProof>;

/// Proof of internal consistency of ReshuffleRekeyVerifiers
class ReshuffleRekeyVerifiersProof {
public:
  ReshuffleRekeyVerifiersProof() = default;
  ReshuffleRekeyVerifiersProof(
    const CurvePoint& rekeyInversePoint,
    const InverseProof& rekeyInverseProof,
    const ScalarMultProof& reshuffleTimesRekeyInverseProof,
    const ScalarMultProof& rekeyTimesPublicKeyProof)
  : rekeyInversePoint(rekeyInversePoint),
    rekeyInverseProof(rekeyInverseProof),
    reshuffleTimesRekeyInverseProof(reshuffleTimesRekeyInverseProof),
    rekeyTimesPublicKeyProof(rekeyTimesPublicKeyProof) {}

  CurvePoint rekeyInversePoint;
  InverseProof rekeyInverseProof;
  ScalarMultProof reshuffleTimesRekeyInverseProof;
  ScalarMultProof rekeyTimesPublicKeyProof;

  static ReshuffleRekeyVerifiersWithProof
  ComputeCertified(
    const CurveScalar& reshuffle,
    const CurveScalar& rekey,
    const ElgamalPublicKey& globalKey);

  void verify(
    const ReshuffleRekeyVerifiers& verifiers,
    const ElgamalPublicKey& globalKey) const;
};


// A compositional non-interactive zero-knowledge proof that
// an ElgamalEncryption (b, c, publicKey) has been RSKed to (b', c', publicKey')
class RskProof {
 public:
  CurvePoint rerandomizePubKey;
  CurvePoint rerandomizePoint;
  ScalarMultProof rerandomizeTimesPubKeyProof; // ScalarMultProof for rerandomize * publicKey
  ScalarMultProof reshuffleOverRekeyTimesBProof; // ScalarMultProof for (reshuffle/rekey) * b
  ScalarMultProof reshuffleTimesCProof; // ScalarMultProof for reshuffle * c

  RskProof() = default;
  RskProof(
    const CurvePoint& rerandomizePubKey,
    const CurvePoint& rerandomizePoint,
    const ScalarMultProof& rerandomizeTimesPubKeyProof,
    const ScalarMultProof& reshuffleOverRekeyTimesBProof,
    const ScalarMultProof& reshuffleTimesCProof)
  : rerandomizePubKey(rerandomizePubKey),
    rerandomizePoint(rerandomizePoint),
    rerandomizeTimesPubKeyProof(rerandomizeTimesPubKeyProof),
    reshuffleOverRekeyTimesBProof(reshuffleOverRekeyTimesBProof),
    reshuffleTimesCProof(reshuffleTimesCProof) {}

  void ensurePacked() const {
    rerandomizePubKey.ensurePacked();
    rerandomizePoint.ensurePacked();
    rerandomizeTimesPubKeyProof.ensurePacked();
    reshuffleOverRekeyTimesBProof.ensurePacked();
    reshuffleTimesCProof.ensurePacked();
  }

  // Constructs a proof that pre is RSKed to post.
  //
  // Assumes reshuffleCommitment = reshuffle*B, reshuffleOverRekey = reshuffle/rekey, reshuffleOverRekeyCommitment = reshuffle/rekey*B,
  // rerandomizePubKey = rerandomize*publicKey, rerandomizePoint = rerandomize*B,
  // and (of course) that post is the RSKed version of pre.
  static RskProof Create(
    const ElgamalEncryption& pre,
    const ElgamalEncryption& post,
    const CurveScalar& reshuffle,
    const CurvePoint& reshuffleCommitment,
    const CurveScalar& reshuffleOverRekey,
    const CurvePoint& reshuffleOverRekeyCommitment,
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
