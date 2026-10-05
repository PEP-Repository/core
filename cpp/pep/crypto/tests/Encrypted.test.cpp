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
  EXPECT_NE(enc.ciphertext, plaintext);
  // We currently rely on this in other parts of the code.
  EXPECT_EQ(enc.ciphertext.size(), Serialization::ToString(Bytes{plaintext}).size())
    << "Cipher text should be the same length as the plaintext serialization";
  EXPECT_GE(enc.tag.size(), 12) << "Tag should not be short";
  EXPECT_EQ(enc.decrypt(key).data, plaintext);

  {
    Encrypted enc2(key, Bytes{plaintext});
    EXPECT_NE(enc2.ciphertext, enc.ciphertext) << "Encryption should be nondeterministic";
  }
  {
    Encrypted enc2 = enc;
    // Modify in the middle: avoid modifying Protobuf stuff & MessageMagic.
    enc2.ciphertext[enc2.ciphertext.size() / 2] ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
      << "Modified ciphertext should not be accepted";
  }
  {
    Encrypted enc2 = enc;
    enc2.iv.front() ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
      << "Modified IV should not be accepted";
  }
  {
    Encrypted enc2 = enc;
    enc2.tag.front() ^= 1;
    EXPECT_THAT([&] { return enc2.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
      << "Modified tag should not be accepted";
  }
}

TEST(Encrypted, decryptShortTag) {
  const std::string key = "abcdefghijklmnopqrstuvwxyz012345";
  const std::string plaintext = "Tuna the cat";
  Encrypted enc(key, Bytes{plaintext});
  ASSERT_EQ(enc.decrypt(key).data, plaintext) << "Decryption sanity check failed";

  enc.tag.pop_back();
  EXPECT_THAT([&] { return enc.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
    << "Shorter tag should not be accepted";
  enc.tag.resize(1);
  EXPECT_THAT([&] { return enc.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
    << "1-byte tag should not be accepted";

  // Now actually modify the ciphertext and forge an encryption.
  // Modify in the middle: avoid modifying Protobuf stuff & MessageMagic.
  enc.ciphertext[enc.ciphertext.size() / 2] ^= 1;
  enc.tag.resize(1);
  for (unsigned char tag = 0;;) {
    if (++tag == 0) { break; }
    enc.tag.front() = static_cast<char>(tag);
    EXPECT_THAT([&] { return enc.decrypt(key).data; }, ThrowsButNotBecauseOfSerialization)
      << "Forged encryption with short tag should not be accepted";
  }
}

}
