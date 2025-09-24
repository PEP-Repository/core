#include <pep/rsk-pep/DataTranslator.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Random.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

using namespace pep;

namespace {

class DataTranslatorTest : public ::testing::Test {
protected:
  static CurveScalar scalar(const std::array<uint8_t, CurveScalar::PACKEDBYTES>& packed) {
    return CurveScalar(SpanToString(packed));
  }

  static KeyFactorSecret factorSecret(const std::array<uint8_t, 64>& packed) {
    return KeyFactorSecret(std::as_bytes(std::span(packed)));
  }

  std::optional<DataTranslator> am, ts;
  ElgamalPrivateKey masterPrivateEncryptionKey;
  ElgamalPublicKey masterPublicEncryptionKey;

  DataTranslatorTest() {
    masterPrivateEncryptionKey = CurveScalar::One();
    {
      const auto masterPrivateEncryptionKeyShare = CurveScalar::Random();
      masterPrivateEncryptionKey = masterPrivateEncryptionKey.mult(masterPrivateEncryptionKeyShare);
      am = DataTranslator(
          {
              .encryptionKeyFactorSecret{RandomArray<64>()},
              .blindingKeySecret{{factorSecret(
                  {0xEB, 0xB9, 0x24, 0xAE, 0x25, 0xD9, 0x4D, 0x4D,
                   0xBF, 0xA7, 0x8A, 0x9F, 0xDC, 0x02, 0x65, 0x73,
                   0x22, 0x43, 0xA3, 0x7A, 0xAB, 0xAD, 0xBF, 0xD8,
                   0x37, 0xCA, 0x4C, 0x45, 0x3D, 0xA2, 0xAA, 0x6D,
                   0x03, 0xC5, 0x40, 0x07, 0xBF, 0xFB, 0xBC, 0x9F,
                   0x9D, 0xB2, 0xCE, 0xC0, 0xA7, 0xBC, 0x81, 0xAA,
                   0xE1, 0xD2, 0xD0, 0x47, 0x73, 0x10, 0x9E, 0xBD,
                   0x28, 0x3E, 0xB2, 0x11, 0xA2, 0x5F, 0xED, 0x81})}},
              .masterPrivateEncryptionKeyShare{std::as_bytes(ToSizedSpan<32>(masterPrivateEncryptionKeyShare.pack()))},
          });
    }
    {
      const auto masterPrivateEncryptionKeyShare = CurveScalar::Random();
      masterPrivateEncryptionKey = masterPrivateEncryptionKey.mult(masterPrivateEncryptionKeyShare);
      ts = DataTranslator(
          {
              .encryptionKeyFactorSecret{RandomArray<64>()},
              .blindingKeySecret{},
              .masterPrivateEncryptionKeyShare{std::as_bytes(ToSizedSpan<32>(masterPrivateEncryptionKeyShare.pack()))},
          });
    }
    masterPublicEncryptionKey = CurvePoint::BaseMult(masterPrivateEncryptionKey);
  }

  void testTranslate(const bool invertBlindKey) {
    const DataTranslator::Recipient user1(1, "User1");

    const std::string blindAddData("AD_A");

    const auto data = CurvePoint::Random();
    const ElgamalEncryption encrypted(masterPublicEncryptionKey, data);
    const auto blinded = am->blind(encrypted, blindAddData, invertBlindKey);
    const auto blindedDecrypt = blinded.decrypt(masterPrivateEncryptionKey);
    auto expectedBlindingKey = scalar(
        {0x7C, 0x27, 0xD6, 0x50, 0x5B, 0x4D, 0x3F, 0x21,
         0x27, 0x2C, 0xC4, 0x9A, 0x7D, 0x3B, 0x8D, 0xCC,
         0xCC, 0x4F, 0xE4, 0xA6, 0x8A, 0x6B, 0x57, 0x7A,
         0xAA, 0xE8, 0xA3, 0xF3, 0x88, 0xA3, 0x7D, 0x06});
    if (invertBlindKey) {
      expectedBlindingKey = expectedBlindingKey.invert();
    }
    const auto expectedBlindedDecrypt = data.mult(expectedBlindingKey);
    EXPECT_EQ(blindedDecrypt, expectedBlindedDecrypt);

    const auto translated1 = am->unblindAndTranslate(blinded, blindAddData, invertBlindKey, user1);
    ASSERT_NE(translated1, blinded);
    const auto translated2 = ts->translateStep(translated1, user1);
    ASSERT_NE(translated2, translated1);
    EXPECT_NE(translated2, encrypted) << "Encryption should be rerandomized";

    const auto sk1 = am->generateKeyComponent(user1).mult(ts->generateKeyComponent(user1));
    ASSERT_NE(sk1, CurveScalar{});
    ASSERT_NE(sk1, CurveScalar::One());
    const auto translatedDecrypt = translated2.decrypt(sk1);
    EXPECT_EQ(translatedDecrypt, data);
  }
};

TEST_F(DataTranslatorTest, translate) {
  testTranslate(true);
}

TEST_F(DataTranslatorTest, translateNonInvertedBlindKey) {
  testTranslate(false);
}

TEST_F(DataTranslatorTest, noShuffleKey) {
  const DataTranslator::Recipient user1(1, "User1");

  const std::string blindAddData("AD_A");

  const ElgamalEncryption encrypted(masterPublicEncryptionKey, CurvePoint::Random());

  EXPECT_THROW((void) ts->blind(encrypted, blindAddData, true), std::logic_error);
  EXPECT_THROW((void) ts->unblindAndTranslate(encrypted, blindAddData, true, user1), std::logic_error);
}

}
