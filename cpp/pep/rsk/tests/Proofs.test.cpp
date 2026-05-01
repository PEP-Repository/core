#include <boost/algorithm/hex.hpp>
#include <gtest/gtest.h>

#include <pep/rsk/Proofs.hpp>

namespace {

TEST(ProofsTest, TestScalarMultProof) {
  for (int i = 0; i < 10; i++) {
    auto x = pep::CurveScalar::Random();
    auto A = x * pep::CurvePoint::Base;
    auto M = pep::CurvePoint::Random();
    auto N = x * M;

    auto proof = pep::ScalarMultProof::Create(A, M, N, x);

    EXPECT_NO_THROW(proof.verify(A, M, N));

    // Test some invalid proofs --- this only catches the most blatant mistakes.
    EXPECT_THROW(proof.verify(M, A, N), pep::InvalidProof) << "Bogus proof should fail to validate";
    EXPECT_THROW(proof.verify(M, N, A), pep::InvalidProof) << "Bogus proof should fail to validate";
  }
}

TEST(ProofsTest, TestRskProof) {
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

}
