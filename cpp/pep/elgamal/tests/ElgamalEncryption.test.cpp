#include <pep/elgamal/ElgamalEncryption.hpp>

#include <gtest/gtest.h>

namespace {

TEST(ElgamalEncryptionTest, FromTextTest) {
  EXPECT_THROW(pep::ElgamalEncryption::FromText(""), std::invalid_argument);
  EXPECT_THROW(pep::ElgamalEncryption::FromText("ABCD:1234"), std::invalid_argument);
}

TEST(ElgamalEncryptionTest, TestKeyGeneration) {
  (void) pep::ElgamalEncryption::CreateKeyPair();
}

TEST(ElgamalEncryptionTest, TestEncipherAndDecipher) {
  auto [private_key, public_key] = pep::ElgamalEncryption::CreateKeyPair();
  pep::CurvePoint test_CurvePoint = pep::CurvePoint::Random();
  pep::CurvePoint test_clone_CurvePoint = {test_CurvePoint};
  EXPECT_EQ(test_CurvePoint, test_clone_CurvePoint);
  pep::ElgamalEncryption enc(public_key, test_CurvePoint);
  EXPECT_NE(enc, enc.rerandomize());
  pep::CurvePoint decrypted_point = enc.decrypt(private_key);
  EXPECT_EQ(test_CurvePoint, decrypted_point);

  //ElgamalEncryption enc(kmaPublicKey, point);
  // //     CurvePoint check1 = enc.decrypt(kmaPrivate);
}

TEST(ElgamalEncryptionTest, ReKeyTest) {
  auto [private_key, public_key] = pep::ElgamalEncryption::CreateKeyPair();
  pep::CurvePoint test_CurvePoint = pep::CurvePoint::Random();
  [[maybe_unused]] pep::CurvePoint test_clone_CurvePoint = {test_CurvePoint};
  pep::CurveScalar test_CurveScalar = pep::CurveScalar::Random();
  pep::ElgamalEncryption enc(public_key, test_CurvePoint);
  pep::ElgamalEncryption enc_clone(enc.b, enc.c, enc.y);
  EXPECT_EQ(enc, enc_clone);
  pep::ElgamalEncryption rekeyed = enc.rekey(pep::ElgamalTranslationKey(test_CurveScalar));
  EXPECT_NE(enc, rekeyed);
}

TEST(ElgamalEncryptionTest, ReShuffleTest) {
  auto [private_key, public_key] = pep::ElgamalEncryption::CreateKeyPair();
  pep::CurvePoint test_CurvePoint = pep::CurvePoint::Random();
  [[maybe_unused]] pep::CurvePoint test_clone_CurvePoint = {test_CurvePoint};
  pep::CurveScalar test_CurveScalar = pep::CurveScalar::Random();
  pep::ElgamalEncryption enc(public_key, test_CurvePoint);
  pep::ElgamalEncryption enc_clone(enc.b, enc.c, enc.y);
  EXPECT_EQ(enc, enc_clone);
  pep::ElgamalEncryption reshuffled = enc.reshuffle(pep::ElgamalTranslationKey(test_CurveScalar));
  EXPECT_NE(enc, reshuffled);
}

TEST(ElgamalEncryptionTest, ReRandomizeTest) {
  auto [private_key, public_key] = pep::ElgamalEncryption::CreateKeyPair();
  pep::CurvePoint test_CurvePoint = pep::CurvePoint::Random();
  [[maybe_unused]] pep::CurvePoint test_clone_CurvePoint = {test_CurvePoint};
  pep::ElgamalEncryption enc(public_key, test_CurvePoint);
  pep::ElgamalEncryption enc_clone(enc.b, enc.c, enc.y);
  EXPECT_EQ(enc, enc_clone);
  pep::ElgamalEncryption reshuffled = enc.rerandomize();
  EXPECT_NE(enc, reshuffled);
}

}
