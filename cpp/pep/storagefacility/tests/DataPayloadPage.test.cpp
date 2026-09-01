#include <pep/storagefacility/DataPayloadPage.hpp>

#include <gtest/gtest.h>

using namespace pep;
using namespace std::literals;

namespace {

// These tests were adapted from Encrypted.test.cpp

TEST(DataPayloadPage, basic) {
  const std::string key = "abcdefghijklmnopqrstuvwxyz012345";
  const std::string plaintext = "Tuna the cat";

  Metadata meta("MyColumn", Timestamp(1234s));

  DataPayloadPage enc;
  enc.setEncrypted(plaintext, key, meta);
  EXPECT_NE(enc.mPayloadData, plaintext);
  // We currently rely on this in other parts of the code.
  EXPECT_EQ(enc.mPayloadData.size(), plaintext.size())
    << "Cipher text should be the same length as the plaintext serialization";
  EXPECT_GE(enc.mCryptoMac.size(), 12) << "Tag should not be short";
  EXPECT_EQ(enc.decrypt(key, meta), plaintext);

  {
    DataPayloadPage enc2;
    enc2.setEncrypted(plaintext, key, meta);
    EXPECT_NE(enc2.mPayloadData, enc.mPayloadData) << "Encryption should be nondeterministic";
  }
  {
    DataPayloadPage enc2 = enc;
    enc2.mPayloadData.front() ^= 1;
    EXPECT_ANY_THROW((void) enc2.decrypt(key, meta))
      << "Modified ciphertext should not be accepted";
  }
  {
    DataPayloadPage enc2 = enc;
    enc2.mCryptoNonce.front() ^= 1;
    EXPECT_ANY_THROW((void) enc2.decrypt(key, meta))
      << "Modified IV should not be accepted";
  }
  {
    DataPayloadPage enc2 = enc;
    enc2.mCryptoMac.front() ^= 1;
    EXPECT_ANY_THROW((void) enc2.decrypt(key, meta))
      << "Modified tag should not be accepted";
  }
  {
    DataPayloadPage enc2 = enc;
    ++enc2.mPageNumber;
    EXPECT_ANY_THROW((void) enc2.decrypt(key, meta))
      << "Modified AD should not be accepted";
  }
}

TEST(DataPayloadPage, decryptShortTag) {
  const std::string key = "abcdefghijklmnopqrstuvwxyz012345";
  const std::string plaintext = "Tuna the cat";

  Metadata meta("MyColumn", Timestamp(1234s));

  DataPayloadPage enc;
  enc.setEncrypted(plaintext, key, meta);
  ASSERT_EQ(enc.decrypt(key, meta), plaintext) << "Decryption sanity check failed";

  enc.mCryptoMac.pop_back();
  EXPECT_ANY_THROW((void) enc.decrypt(key, meta)) << "Shorter tag should not be accepted";
  enc.mCryptoMac.resize(1);
  EXPECT_ANY_THROW((void) enc.decrypt(key, meta)) << "1-byte tag should not be accepted";

  enc.mPayloadData.front() ^= 1;
  enc.mCryptoMac.resize(1);
  for (unsigned char tag = 0;;) {
    if (++tag == 0) { break; }
    enc.mCryptoMac.front() = static_cast<char>(tag);
    EXPECT_ANY_THROW((void) enc.decrypt(key, meta)) << "Forged encryption with short tag should not be accepted";
  }
}

}
