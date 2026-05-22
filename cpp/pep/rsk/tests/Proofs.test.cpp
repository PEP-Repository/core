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
    evilVerifiers.mRekeyedPublicKey = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong rekeyedPublicKey";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.mRekeyCommitment = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong rekeyCommitment";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.mReshuffleCommitment = pep::CurvePoint::Random();
    EXPECT_THROW(proof.verify(evilVerifiers, globalKey),
      pep::InvalidProof) << "Proof should fail to validate with wrong reshuffleCommitment";
  }
  {
    auto evilVerifiers = verifiers;
    evilVerifiers.mReshuffleOverRekeyCommitment = pep::CurvePoint::Random();
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

TEST(Proofs, RerandomizeProof) {
  for (int i = 0; i < 10; i++) {
    const pep::ElgamalEncryption pre(
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random());
    {
      pep::ElgamalEncryption post;
      auto proof = pep::RerandomizeProof::CertifiedRerandomize(pre, post);

      EXPECT_NO_THROW(proof.verify(pre, post));

      // Test some invalid proofs --- this only catches the most blatant mistakes.
      //NOLINTNEXTLINE(readability-suspicious-call-argument) pre & post intentionally swapped
      EXPECT_THROW(proof.verify(post, pre), pep::InvalidProof) << "Bogus proof should fail to validate";
    }

    {
      auto rerandomize = pep::CurveScalar::Random();
      auto rerandomizePoint = rerandomize * pep::CurvePoint::Base;
      auto rerandomizePubKey = rerandomize * pre.publicKey;
      pep::ElgamalEncryption rerandomized{
        pre.b + rerandomizePoint,
        pre.c + rerandomizePubKey,
        pre.publicKey,
      };

      auto base = pep::CurveScalar::One() * pep::CurvePoint::Base;

      auto incorrectlyRerandomizedB = rerandomized;
      incorrectlyRerandomizedB.b = pre.b; // Intentionally wrong
      auto incorrectlyRerandomizedC = rerandomized;
      incorrectlyRerandomizedC.c = pre.c + rerandomizePoint; // Intentionally wrong
      auto incorrectlyRerandomizedPublicKey = rerandomized;
      incorrectlyRerandomizedPublicKey.publicKey = base; // Intentionally wrong

      auto proof = pep::RerandomizeProof::Create(pre.publicKey, rerandomize, rerandomizePubKey, rerandomizePoint);
      EXPECT_NO_THROW(proof.verify(pre, rerandomized));
      // Make sure we check post.b
      EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedB), pep::InvalidProof)
        << "Proof with inconsistent post ciphertext B should fail to validate";
      // Make sure we check post.c
      EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedC), pep::InvalidProof)
        << "Proof with inconsistent post ciphertext C should fail to validate";
      // Make sure that proof checks that public key did not change
      EXPECT_THROW(proof.verify(pre, incorrectlyRerandomizedPublicKey), pep::InvalidProof)
        << "Proof with inconsistent post ciphertext public key should fail to validate";

      {
        // Construct RerandomizeProof which is correct except we use Base instead of publicKey
        auto incorrectProof = pep::RerandomizeProof::Create(base, rerandomize, rerandomizePoint, rerandomizePoint);
        EXPECT_NO_THROW(incorrectProof.mRerandomizeTimesPubKeyProof.verify(rerandomizePoint, base, rerandomizePoint))
          << "Useless scalar mult proof should still be correct";
        // Either ScalarMultProof should fail to validate, or it fails on pre.publicKey != post.publicKey
        EXPECT_THROW(incorrectProof.verify(pre, incorrectlyRerandomizedC), pep::InvalidProof)
          << "Proof using rerandomizePoint for C should fail to validate";

        // Make sure that proof checks that public key did not change also if scalar mult proof verification uses pre.publicKey
        auto preIncorrectPublicKey = pre;
        preIncorrectPublicKey.publicKey = base;
        EXPECT_THROW(incorrectProof.verify(preIncorrectPublicKey, incorrectlyRerandomizedC), pep::InvalidProof)
          << "Proof using rerandomizePoint for C and only pre publicKey consistent with scalar mult proof should fail to validate";

        // Make sure that proof checks that public key did not change also if scalar mult proof verification uses post.publicKey
        incorrectlyRerandomizedC.publicKey = base;
        EXPECT_THROW(incorrectProof.verify(pre, incorrectlyRerandomizedC), pep::InvalidProof)
          << "Proof using rerandomizePoint for C and only post publicKey consistent with scalar mult proof should fail to validate";
      }
    }
  }
}

}
