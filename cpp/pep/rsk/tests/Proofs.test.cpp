#include <boost/algorithm/hex.hpp>
#include <gtest/gtest.h>

#include <pep/rsk/Proofs.hpp>

namespace {

TEST(Proofs, ScalarMultProof) {
  for (int i = 0; i < 10; i++) {
    auto x = pep::CurveScalar::Random();
    auto A = x * pep::CurvePoint::Base;
    auto M = pep::CurvePoint::Random();
    auto N = x * M;

    auto proof = pep::ScalarMultProof::Create(A, M, N, x);

    EXPECT_NO_THROW(proof.verify(A, M, N));

    // Test some invalid proofs --- this only catches the most blatant mistakes.
    EXPECT_THROW(proof.verify(M, A, N), pep::InvalidProof) << "Proof should fail to validate with bogus verifiers";
    EXPECT_THROW(proof.verify(M, N, A), pep::InvalidProof) << "Proof should fail to validate with bogus verifiers";
  }
}

TEST(Proofs, InverseProof) {
  const auto secret = pep::CurveScalar::Random();
  const auto secretAsPoint = secret * pep::CurvePoint::Base;
  const auto secretInverse = secret.invert();
  const auto secretInversePoint = secretInverse * pep::CurvePoint::Base;

  const auto proof = pep::InverseProof::Create(secretInversePoint, secretAsPoint, secretInverse);
  EXPECT_NO_THROW(proof.verify(secretInversePoint, secretAsPoint));

  // Test some invalid proofs --- this only catches the most blatant mistakes:
  //NOLINTNEXTLINE(readability-suspicious-call-argument) verify parameters intentionally swapped
  EXPECT_THROW(proof.verify(secretAsPoint, secretInversePoint),
    pep::InvalidProof) << "Proof should fail to validate with bogus verifiers";
  // Test inverse unrelated to secret
  EXPECT_THROW(pep::InverseProof(pep::ScalarMultProof::Create(
      pep::CurvePoint{}, secretAsPoint, pep::CurvePoint{}, pep::CurveScalar{}))
    .verify(pep::CurvePoint{}, secretAsPoint),
    pep::InvalidProof) << "Bogus proof should fail to validate";
}

TEST(Proofs, ReshuffleRekeyVerifiersProof) {
  auto globalKey = pep::CurvePoint::Random();
  const auto [verifiers, proof] = pep::ReshuffleRekeyVerifiersProof::ComputeCertified(
    pep::CurveScalar::Random(),
    pep::CurveScalar::Random(),
    globalKey);

  EXPECT_NO_THROW(proof.verify(verifiers, globalKey));

  // Test some invalid proofs
  EXPECT_THROW(proof.verify(verifiers, pep::CurvePoint::Random()),
    pep::InvalidProof) << "Proof should fail to validate with wrong global key";
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.rekeyedPublicKey_ = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong rekeyedPublicKey";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.rekeyCommitment_ = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong rekeyCommitment";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.reshuffleCommitment_ = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong reshuffleCommitment";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.reshuffleOverRekeyCommitment_ = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong reshuffleOverRekeyCommitment";
  }
}

TEST(Proofs, RskProof) {
  for (int i = 0; i < 10; i++) {
    auto pre = pep::ElgamalEncryption(
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random());
    pep::ElgamalEncryption post;
    auto rekey = pep::CurveScalar::Random();
    auto reshuffle = pep::CurveScalar::Random();

    auto proof = pep::RskProof::CertifiedRsk(pre, post, reshuffle, rekey);

    EXPECT_NO_THROW(proof.verify(
      pre,
      post,
      pep::ReshuffleRekeyVerifiers::Compute(reshuffle, rekey, pre.publicKey)
    ));

    // Test some invalid proofs --- this only catches the most blatant mistakes.
    EXPECT_THROW(proof.verify( //NOLINT(readability-suspicious-call-argument) pre & post intentionally swapped
      post, pre,
      pep::ReshuffleRekeyVerifiers::Compute(reshuffle, rekey, pre.publicKey)
    ), pep::InvalidProof) << "Bogus proof should fail to validate";
    EXPECT_THROW(proof.verify( //NOLINT(readability-suspicious-call-argument) pre & post intentionally swapped
      post, pre,
      //NOLINTNEXTLINE(readability-suspicious-call-argument) reshuffle & rekey intentionally swapped
      pep::ReshuffleRekeyVerifiers::Compute(rekey, reshuffle, pre.publicKey)
    ), pep::InvalidProof) << "Bogus proof should fail to validate";
  }
}

// Test rerandomize part of RskProof.
// Adapted from RerandomizeProof tests from reverted https://gitlab.pep.cs.ru.nl/pep/core/-/merge_requests/2394.
//TODO Expand to make sure reshuffle/rekey commitments are also used.
TEST(Proofs, RskProof_rerandomize) {
  const pep::ElgamalEncryption pre(
      pep::CurvePoint::Random(),
      pep::CurvePoint::Random(),
      pep::CurvePoint::Random());
  auto rerandomize = pep::CurveScalar::Random();
  auto rerandomizePoint = rerandomize * pep::CurvePoint::Base;
  auto rerandomizePubKey = rerandomize * pre.publicKey;
  pep::ElgamalEncryption rerandomized{
    pre.b + rerandomizePoint,
    pre.c + rerandomizePubKey,
    pre.publicKey,
  };

  const auto verifiers = pep::ReshuffleRekeyVerifiers::Compute(
    pep::CurveScalar::One(), pep::CurveScalar::One(),
    pre.publicKey);

  auto base = pep::CurveScalar::One() * pep::CurvePoint::Base;

  auto incorrectlyRerandomizedB = rerandomized;
  incorrectlyRerandomizedB.b = pre.b; // Intentionally wrong
  auto incorrectlyRerandomizedC = rerandomized;
  incorrectlyRerandomizedC.c = pre.c + rerandomizePoint; // Intentionally wrong
  auto incorrectlyRerandomizedPublicKey = rerandomized;
  incorrectlyRerandomizedPublicKey.publicKey = base; // Intentionally wrong

  auto proof = pep::RskProof::Create(pre, rerandomized,
    pep::CurveScalar::One(), base,
    pep::CurveScalar::One(), base,
    rerandomize, rerandomizePubKey, rerandomizePoint);
  EXPECT_NO_THROW(proof.verify(pre, rerandomized, verifiers));
  // Make sure we check post.b
  EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedB, verifiers), pep::InvalidProof)
    << "Proof with inconsistent post ciphertext B should fail to validate";
  // Make sure we check post.c
  EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedC, verifiers), pep::InvalidProof)
    << "Proof with inconsistent post ciphertext C should fail to validate";
  // Make sure that proof checks that public key is correct
  EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedPublicKey, verifiers), pep::InvalidProof)
    << "Proof with inconsistent post ciphertext public key should fail to validate";
}

}
