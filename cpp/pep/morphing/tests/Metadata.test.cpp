#include <gtest/gtest.h>

#include <pep/morphing/Metadata.hpp>
#include <pep/utils/Random.hpp>

namespace {

TEST(MetadataTest, encryptionTest) {
  const pep::MetadataXEntry me = pep::MetadataXEntry::FromPlaintext("Hi there!", true, true);
  EXPECT_TRUE(me.storeEncrypted());
  EXPECT_FALSE(me.isEncrypted());

  // Encrypting should work
  const std::string key = pep::RandomString(32);
  const auto meEnc = me.prepareForStore(key);
  EXPECT_TRUE(meEnc.isEncrypted());
  EXPECT_NE(meEnc.payloadForStore(), me.plaintext());
  EXPECT_EQ(meEnc.bound(), me.bound());

  // Decrypting should work
  const auto meEncDec = meEnc.preparePlaintext(key);
  EXPECT_FALSE(meEncDec.isEncrypted());
  EXPECT_TRUE(meEncDec.storeEncrypted());
  EXPECT_EQ(meEncDec.plaintext(), me.plaintext());
  EXPECT_EQ(meEncDec.bound(), me.bound());

  // Encrypting twice should not do anything
  const auto meEncEnc = meEnc.prepareForStore(key);
  EXPECT_TRUE(meEncEnc.isEncrypted());
  EXPECT_EQ(meEncEnc.payloadForStore(), meEnc.payloadForStore());

  // Decrypting plaintext should not do anything
  const auto meDec = me.preparePlaintext(key);
  EXPECT_FALSE(meDec.isEncrypted());
  EXPECT_TRUE(meDec.storeEncrypted());
  EXPECT_EQ(meDec.plaintext(), me.plaintext());

  // Asking for payload form not contained in entry should throw
  EXPECT_ANY_THROW((void)me.payloadForStore());
  EXPECT_ANY_THROW((void)meEnc.plaintext());
}

TEST(MetadataTest, noEncryptionTest) {
  const pep::MetadataXEntry me = pep::MetadataXEntry::FromPlaintext("Hi there!", false, true);
  EXPECT_FALSE(me.storeEncrypted());
  EXPECT_FALSE(me.isEncrypted());

  // Encrypting should not do anything
  const std::string key = pep::RandomString(32);
  const auto meEnc = me.prepareForStore(key);
  EXPECT_FALSE(meEnc.isEncrypted());
  EXPECT_EQ(meEnc.payloadForStore(), me.plaintext());

  EXPECT_NO_THROW((void)me.payloadForStore());
}

}
