#include <boost/algorithm/hex.hpp>
#include <gtest/gtest.h>

#include <pep/rsk/Proofs.hpp>

namespace {

TEST(ProofsTest, TestScalarMultProof) {
  for (int i = 0; i < 100; i++) {
    auto x = pep::CurveScalar::Random();
    auto A = pep::CurvePoint::BaseMult(x);
    auto M = pep::CurvePoint::Random();
    auto N = M.mult(x);

    auto proof = pep::ScalarMultProof::create(A, M, N, x);

    proof.verify(A, M, N);

    // Test some invalid proofs --- this only catches the most blatant mistakes.
    // XXX Disabled per #777
    // EXPECT_ANY_THROW(proof.verify(M, A, N));
    // EXPECT_ANY_THROW(proof.verify(M, N, A));
  }
}

TEST(ProofsTest, TestRSKProof) {
  for (int i = 0; i < 100; i++) {
    auto pre = pep::ElgamalEncryption(
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random(),
        pep::CurvePoint::Random());
    pep::ElgamalEncryption post;
    auto k = pep::CurveScalar::Random();
    auto z = pep::CurveScalar::Random();

    auto proof = pep::RSKProof::certifiedRSK(pre, post, z, k);

    proof.verify(
      pre,
      post,
      pep::RSKVerifiers::compute(z, k, pre.y)
    );

    // Test some invalid proofs --- this only catches the most blatant mistakes.
    // XXX Disabled per #777
    // EXPECT_ANY_THROW(proof.verify(
    //   post,
    //   pre,
    //   pep::RSKVerifiers::compute(k, z, pre.y)
    // ));
  }
}

}
