#include <pep/crypto/Encrypted.hpp>
#include <pep/crypto/Bytes.hpp>
#include <pep/crypto/BytesSerializer.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace pep;
using testing::Throws;

namespace {

//TODO We should probably introduce an exception type for decryption errors.
const auto ThrowsButNotBecauseOfSerialization = AllOf(Throws<std::exception>(), Not(Throws<SerializeException>()));

TEST(Encrypted, basic) {
  const std::string key = "abcdefghijklmnopqrstuvwxyz012345";
  const std::string plaintext = "Tuna the cat";

  Encrypted enc(key, Bytes{plaintext});
  EXPECT_NE(enc.mCiphertext, plaintext);
  // We currently rely on this in other parts of the code.
  EXPECT_EQ(enc.mCiphertext.size(), Serialization::ToString(Bytes{plaintext}).size())
    << "Cipher text should be the same length as the plaintext serialization";
  EXPECT_GE(enc.mTag.size(), 12) << "Tag should not be short";
  EXPECT_EQ(enc.decrypt(key).mData, plaintext);

  {
    Encrypted enc2(key, Bytes{plaintext});
    EXPECT_NE(enc2.mCiphertext, enc.mCiphertext) << "Encryption should be nondeterministic";
  }
  {
    Encrypted enc2 = enc;
    // Modify in the middle: avoid modifying Protobuf stuff & MessageMagic.
    enc2.mCiphertext[enc2.mCiphertext.size() / 2] ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
      << "Modified ciphertext should not be accepted";
  }
  {
    Encrypted enc2 = enc;
    enc2.mIv.front() ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
      << "Modified IV should not be accepted";
  }
  {
    Encrypted enc2 = enc;
    enc2.mTag.front() ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
      << "Modified tag should not be accepted";
  }
}

TEST(Encrypted, decryptShortTag) {
  const std::string key = "abcdefghijklmnopqrstuvwxyz012345";
  const std::string plaintext = "Tuna the cat";
  Encrypted enc(key, Bytes{plaintext});
  ASSERT_EQ(enc.decrypt(key).mData, plaintext) << "Decryption sanity check failed";

  enc.mTag.pop_back();
  EXPECT_THAT([&] { return enc.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
    << "Shorter tag should not be accepted";
  enc.mTag.resize(1);
  EXPECT_THAT([&] { return enc.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
    << "1-byte tag should not be accepted";

  // Now actually modify the ciphertext and forge an encryption.
  // Modify in the middle: avoid modifying Protobuf stuff & MessageMagic.
  enc.mCiphertext[enc.mCiphertext.size() / 2] ^= 1;
  enc.mTag.resize(1);
  for (unsigned char tag = 0;;) {
    if (++tag == 0) { break; }
    enc.mTag.front() = static_cast<char>(tag);
    EXPECT_THAT([&] { return enc.decrypt(key).mData; }, ThrowsButNotBecauseOfSerialization)
      << "Forged encryption with short tag should not be accepted";
  }
}

}
