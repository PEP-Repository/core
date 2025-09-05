#include <pep/rsk-pep/Pseudonyms.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace pep;

namespace {

TEST(Pseudonyms, zeroPoint) {
  const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
  const std::string packedZero(CurvePoint{}.pack());
  EXPECT_THROW((void) LocalPseudonym::FromPacked(packedZero).encrypt(pk), std::invalid_argument);

  const std::string packedEncZeroPk =
      std::string(CurvePoint::Random().pack()) +
      std::string(CurvePoint::Random().pack()) +
      packedZero;
  EXPECT_THROW((void) EncryptedLocalPseudonym::FromPacked(packedEncZeroPk), std::invalid_argument);
  EXPECT_THROW((void) PolymorphicPseudonym::FromPacked(packedEncZeroPk), std::invalid_argument);
}

TEST(Pseudonyms, encryptDecryptLocal) {
  const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
  const auto local = LocalPseudonym::Random();
  const auto encrypted = local.encrypt(pk);
  const auto decrypted = encrypted.decrypt(sk);
  EXPECT_EQ(decrypted, local);
}

TEST(Pseudonyms, encryptDecryptPolymorph) {
  const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
  const std::string id("PEP1234");
  const auto polymorph = PolymorphicPseudonym::FromIdentifier(pk, id);
  const auto decrypted = ElgamalEncryption::FromText(polymorph.text()).decrypt(sk);
  const auto idPoint = CurvePoint::Hash(id);
  EXPECT_EQ(decrypted, idPoint);
}

TEST(Pseudonyms, packUnpackEncryption) {
  const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
  const auto local = LocalPseudonym::Random();
  const auto encrypted = local.encrypt(pk);
  const auto encryptedFromPack = EncryptedLocalPseudonym::FromPacked(encrypted.pack());
  EXPECT_EQ(encryptedFromPack, encrypted);
}

TEST(Pseudonyms, nonDeterminism) {
  const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
  {
    const auto local = LocalPseudonym::Random();
    const auto encrypted1 = local.encrypt(pk);
    const auto encrypted2 = local.encrypt(pk);
    EXPECT_NE(encrypted1, encrypted2) << "Encrypting a local pseudonym should be non-deterministic";
  }
  {
    const auto [sk, pk] = ElgamalEncryption::CreateKeyPair();
    const std::string id("PEP1234");
    const auto polymorph1 = PolymorphicPseudonym::FromIdentifier(pk, id);
    const auto polymorph2 = PolymorphicPseudonym::FromIdentifier(pk, id);
    EXPECT_NE(polymorph1, polymorph2) << "Generating a polymorphic pseudonym should be non-deterministic";
  }
}

}
