#include <gtest/gtest.h>

#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/utils/Sha.hpp>

const std::string inputPublicKey1 = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAwaMF5e3GIDgyqUN/O4Y3\n\
FHXrmXBIJ+jLfas0Nkd8ckPdYQbRB5EhlX6DzraEvGzKpoox+Z1hCO6+QIBclek8\n\
3oNFbK63iB+iBz8xm+uMm4OkG+CW/BAnCmoM1gciM74OZgHcCSHM+zbHTQUSidqj\n\
tDbrpAHATN4nOwlE413ACkSq5nf7v4oJUKRLtLhL7xei5EKbP8Tm/4sK7wjitnoJ\n\
50TqY9CqBMfDMV0IkiJkaMF+LCoj5E0zkcNwwR39D924ra92w90rEityodsF819U\n\
IvRR2Whx6UelmLdPoxt34DJrUM0SJmgvpi1P2seSF2CS0zK8VnCJM11G1Tosy7R5\n\
sQIDAQAB\n\
-----END PUBLIC KEY-----\n";

const std::string inputPrivateKey1 = "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEowIBAAKCAQEAwaMF5e3GIDgyqUN/O4Y3FHXrmXBIJ+jLfas0Nkd8ckPdYQbR\n\
B5EhlX6DzraEvGzKpoox+Z1hCO6+QIBclek83oNFbK63iB+iBz8xm+uMm4OkG+CW\n\
/BAnCmoM1gciM74OZgHcCSHM+zbHTQUSidqjtDbrpAHATN4nOwlE413ACkSq5nf7\n\
v4oJUKRLtLhL7xei5EKbP8Tm/4sK7wjitnoJ50TqY9CqBMfDMV0IkiJkaMF+LCoj\n\
5E0zkcNwwR39D924ra92w90rEityodsF819UIvRR2Whx6UelmLdPoxt34DJrUM0S\n\
Jmgvpi1P2seSF2CS0zK8VnCJM11G1Tosy7R5sQIDAQABAoIBADl/9G9rpQrKRVj3\n\
/x8o4tBDl/uPWQ3o1gxyO2Xm4nB38JQwVv/9O9DNqcxHbLEbSS4dGWv7LOZfJsW2\n\
mEe34+hbaNE2LK/SXOX0AQJA9xbzB1dz4MHm9gDkrv0bTy+4P0RRRwq7K8hpYtNf\n\
LzsaXsRUDrM8BeONkG66eOdfXnhtmNXmPP048B5iZ3Id0kYCFfulgRIzWRHuK261\n\
3hrz3YdMySXv4ReZnyZeCDKACecSspd+gZauuxIZI4bUIa/bvZa2u6J8N12KfAT1\n\
Pl3tQZCUXe6t62BhlftErMDxJmjG/oquZCoSlUG4xEXsHXsTDkpZChu9DnaFxYIa\n\
vEHh4TECgYEA9JKlEKzHSS3qHRDNA2PzscTHUt1bgHW8wrhe4BCs80AR+GvyzogR\n\
pt6JHuOm1wppp8g3sy6frOzgqAQJdFq1iR3KwrFmQToAAOujw2zA1847/ufPIGTo\n\
25BezDvjyvqcIx9p+2dXwh+EIOCXeOLNgZ/X2Av1CAtUsZWA7Us82RUCgYEAyq8g\n\
35VVWmT3bo0eaOtE1Aq3o6498pc7v8omCJUsmNq1PBj55wRxg1ztlT+38HXDbsGy\n\
reOLHfMWMmjDpz1JlEe8k9qWCfvs902e8IFV/3GzR3IzCpV/eS/XEpT7ucNNEqvl\n\
XCt179gCEZFheabalV8x1/sysQ64ncZRQcGbTS0CgYApSu0VoKZRA7CIUcLbdK+7\n\
bubcZcVCLh69nZVfLVGWDFY8ZDVti2m3i9EI5xTPL9Hg4xwMY63P63qOw4e5HmuS\n\
B/ao4nzKPHmtrhtLLnxss0RL6GV/KaprD7gBsYbnSWK9R1uEd9FIVDvhtSm93kUm\n\
Qo+VyYcIYaleBkjrR42xdQKBgFWXt240BiNyV/tbpOfx0tMo43w/7PExZI4NtBoT\n\
xQ2X7sk+Uup4OeebqslIa0kksi9npSlB2lH/gfQvwdAyVYxE7yIRQSNePCgDo4c1\n\
VzfUsD0PwPZLQ9XNminCuLst+rJT3TwbLmbm9ZitqFhTWiOSW941uqaC7PvT8CSw\n\
ugDRAoGBAMUFit7yXS9F6ThrNNCYZSeVXZo9sXp4e2FflBpjYHZczciPMuUl5L+q\n\
BQskE3EnU7n55j17qtF8867IdsXsjQudib5pnVfzHBsr6H3KzupPBS43J1vyw5fx\n\
9EtwCn79gXrjDbOlqEn9VzVKhnqBBHsh0ciWX6v9z7BIa2JX+t7w\n\
-----END RSA PRIVATE KEY-----\n";

const std::string inputPublicKey2 = "-----BEGIN PUBLIC KEY-----\n\
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtvOGfpbZiKDOR6jl6wnx\n\
Y9QdX0blVNlLS9OWunzv3a2+Oeaw3F+kwxh0R+qPfBOaNO3muW9stWNVw6zwAnNY\n\
zOrGjz9LMcCVxeWir8m4Z+wYbhq/jav4+arHRFFGDmOGbl+a2TSFQe1eEOdmQD2r\n\
0n/shZBLViMWlq72M0VfttViOBwgBgxSSQEDM1LNQ7rpFNzyCLo+Y3HyK6b0EWth\n\
K+4CszlNKTupZxk1G2lfhmYYI5a2VMG479epHPfcOCUfZwco9OlgYW7GOvktqWO4\n\
sj9N4RC/AmDYg2CN2md9qwVo7wJJuLwGzxBZU1o/JQpcIwJx2rXaP3VegYTkuUPq\n\
VQIDAQAB\n\
-----END PUBLIC KEY-----\n";

const std::string inputPrivateKey2 = "-----BEGIN RSA PRIVATE KEY-----\n\
MIIEogIBAAKCAQEAtvOGfpbZiKDOR6jl6wnxY9QdX0blVNlLS9OWunzv3a2+Oeaw\n\
3F+kwxh0R+qPfBOaNO3muW9stWNVw6zwAnNYzOrGjz9LMcCVxeWir8m4Z+wYbhq/\n\
jav4+arHRFFGDmOGbl+a2TSFQe1eEOdmQD2r0n/shZBLViMWlq72M0VfttViOBwg\n\
BgxSSQEDM1LNQ7rpFNzyCLo+Y3HyK6b0EWthK+4CszlNKTupZxk1G2lfhmYYI5a2\n\
VMG479epHPfcOCUfZwco9OlgYW7GOvktqWO4sj9N4RC/AmDYg2CN2md9qwVo7wJJ\n\
uLwGzxBZU1o/JQpcIwJx2rXaP3VegYTkuUPqVQIDAQABAoIBABYj1GPfZ4XkR/Je\n\
GyzdcCvvkHpmPvyMq1MK0RPSaMi/7ORe6YpRvMOrYu8NEL4oNSIwpBpOxK4Szl82\n\
v3jccqOhydOuCjCEKNvhFVYGqF1TMgWpEQNZC3FTXHgFCeBV5P/YbAnbFEFNM3QC\n\
PNqLXA7GUl47fxJ2fpZPqBW+UH30kjI9rPWMdsw3SIxMU565sRPji0Je1DsFvmKz\n\
CYCcLMEjvVOx4/sDGs1g17hvC7LQUBVhUrHUjRWsbYzT6jOl9sf/pmdyC3zTfCbY\n\
W8bf3/c9JWQiH4SwR+qwXmG+x9RlGmYU1qX6w44MadUIsWYwTzvYeLmaw2HD6CFE\n\
w/mBhSECgYEA6+IVwGblymB1KyJkfc9iG3/A5kQgx9fDRXq38ZPx1QvK/UuRPvO6\n\
+AMmJg/5lZXa0K5ogJPiqzJi6y6eM1R543SJZEgZJwSbbYYS0NPaxlVVyqlk1dnq\n\
AqmzMQDMppbwRgf8hJ6hEF1Xwcfy5MJRcbfxeqY9r0SjhIio/0uH4W0CgYEAxo3O\n\
XTGNOsX5Nfo+RECP8OBUl8tgBTiQ9xxf8gHb40yh/UG46lGMxWMvrKa0DaoMM89i\n\
sTadbzatWdwZEdcr+pWtTSUh/Me7hpiK/qNis7T2ynqqNhVk5rud5iid/CDd8moQ\n\
sFIQEU5QC8wCFO0kOk1hMPsYTS2FEIRWWkSIA4kCgYBsZBiAsAfZxhcxOf2Zfklj\n\
v4HBjf7ONgxqCekqnkQbFO8zE51row4AV1oZVW/n19OT3wDwTIR1DJM95M8XYTMd\n\
XPihVywPrONLIbfVs/Qs/RuOI+bNCfSpQpev5eEkj+lbFOJpgocagPoJdrrbeZt5\n\
OQBCzs87kbvd8/pMTcXjxQKBgAP5ksgK1ej3TaXm/JghMsB/vTHMwH9aQoyv5LvT\n\
jbNfNV78kdcfCtJoyeuK6s/bN6NR44for/4p+g5yeY4B4L+Df5SryaJl3Ts0kpPG\n\
cZNnbAlhq0ap5vs3hlG1PnRttAPGW88r1WaDStbxnpkMpk0Ef42beUESSDesbo4g\n\
ERkBAoGAVunEU/yV7uJbRrKL5pSD2yYmvLCs19GF6+312U0ZQRJV30CBGLmi2GE8\n\
Z1GvKsSTiKzej8tlWxWCVcbJGuH92LWcZ1SWZIC5HXf8aNAoEcNGZ8pUzurWJy/w\n\
t9jn4n8oS5x/IUZaWdfoEJ6p6FLlx/DvtubHnZzd7AGv1NmDdk0=\n\
-----END RSA PRIVATE KEY-----\n";

const std::string inputPublicKey3 = "-----BEGIN RSA PUBLIC KEY-----\n\
MIIBCgKCAQEAzp1LfyLv0hMO0pBvSJoepGjMq5ntS6NCYcboZJbuZsq2UAx70M6o\n\
RtN6oHxtdwvFR9stSNq6W3fp/uD28GMpTK1KISepV6BGS+cwcRG8Jh7eyoKCZhcn\n\
yAcOmcIoZGmEgXry6DJLE+lTPtIl8ycd5StkzOu29xr5GxwXk0Q/kUGAUJ4vcGGV\n\
U+/0YEAJZZzwMPoY++O/gKmkycQXx5D5yo7FaQHjt30PFMQzOEylzAvka8hbJtxN\n\
hI565udST4I/2SCL+g4tV6NO2+grfPA8z8PUU90ww0fkTyPEptViOtDxEYgrfsHz\n\
O8jvbjJHWt3x0ZF2QbJU7K09vJ33uTnwtQIDAQAB\n\
-----END RSA PUBLIC KEY-----\n";

TEST(AsymmetricKeyTest, TestEncryption) {
  pep::AsymmetricKey privateKey1 = pep::AsymmetricKey(inputPrivateKey1);
  pep::AsymmetricKey publicKey1 = pep::AsymmetricKey(inputPublicKey1);
  pep::AsymmetricKey privateKey2 = pep::AsymmetricKey(inputPrivateKey2);
  pep::AsymmetricKey publicKey2 = pep::AsymmetricKey(inputPublicKey2);

  std::string testData = "TestData";
  std::string encryptedData1 = publicKey1.encrypt(testData);
  std::string encryptedData2 = publicKey2.encrypt(testData);
  EXPECT_NE(encryptedData1, encryptedData2);

  EXPECT_NE(encryptedData1, "");

  std::string decryptedData1 = privateKey1.decrypt(encryptedData1);
  EXPECT_EQ(testData, decryptedData1);

  std::string decryptedData2 = privateKey2.decrypt(encryptedData2);
  EXPECT_EQ(testData, decryptedData2);
}

TEST(AsymmetricKeyTest, TestSignatures) {
  pep::AsymmetricKey privateKey1 = pep::AsymmetricKey(inputPrivateKey1);
  pep::AsymmetricKey publicKey1 = pep::AsymmetricKey(inputPublicKey1);
  pep::AsymmetricKey privateKey2 = pep::AsymmetricKey(inputPrivateKey2);
  pep::AsymmetricKey publicKey2 = pep::AsymmetricKey(inputPublicKey2);

  // We need some serializable data
  std::string testData1 = pep::Sha256().digest("Some test");
  std::string testData2 = pep::Sha256().digest("Some other test");

  std::string signature = privateKey1.signDigest(testData1);
  EXPECT_NE("", signature);

  bool check;
  check = publicKey1.verifyDigest(testData1, signature);
  EXPECT_TRUE(check);

  check = publicKey1.verifyDigest(testData2, signature);
  EXPECT_FALSE(check);

  check = publicKey2.verifyDigest(testData1, signature);
  EXPECT_FALSE(check);

  // Provided digest
  std::vector<unsigned char> digestVector = {
    0x88, 0x51, 0xc7, 0xf9, 0xd9, 0xb4, 0x91, 0x41, 0xd8, 0x29, 0x2c, 0xa9, 0xd1, 0x82, 0xb2, 0xd4,
    0xd6, 0x5b, 0x8d, 0xe0, 0xab, 0x07, 0xaf, 0x8c, 0x50, 0xc4, 0x24, 0x79, 0x9a, 0xa5, 0x86, 0xd8
  };

  // Provided signature
  std::vector<unsigned char> signatureVector = {
    0x12, 0xcf, 0xb6, 0x04, 0xd7, 0xeb, 0x21, 0xed, 0xfe, 0x45, 0x04, 0x5c, 0x04, 0x20, 0xe9, 0x10,
    0x74, 0x03, 0x04, 0x8e, 0xfd, 0xf2, 0xd7, 0xb0, 0x94, 0xb0, 0x09, 0x2d, 0xfd, 0xd0, 0xdf, 0xd7,
    0xd2, 0x99, 0x3f, 0x4c, 0x47, 0xfd, 0x6b, 0xee, 0x49, 0xf2, 0x91, 0xdf, 0x1d, 0x50, 0x00, 0x66,
    0x64, 0x70, 0x31, 0x4b, 0xf9, 0x54, 0x6a, 0x05, 0x99, 0x91, 0x4d, 0xee, 0x13, 0x1e, 0x4a, 0x3b,
    0x87, 0xbf, 0xfe, 0x41, 0x41, 0x65, 0x07, 0x01, 0xbd, 0x83, 0x95, 0x76, 0xc9, 0xf9, 0x67, 0x4d,
    0x51, 0x14, 0x81, 0x1b, 0x12, 0x3d, 0x79, 0x1e, 0xc8, 0xb5, 0x15, 0xa0, 0xb4, 0x93, 0x02, 0x33,
    0xa4, 0xcb, 0x37, 0x8e, 0xdd, 0x98, 0x14, 0x46, 0xc2, 0x5d, 0xfa, 0xd3, 0xf4, 0x4f, 0x63, 0x51,
    0x65, 0x63, 0x47, 0xc3, 0x4b, 0xaa, 0xdc, 0x56, 0x3d, 0x01, 0xcc, 0x8a, 0xa8, 0x4c, 0x87, 0x4f,
    0xe2, 0x78, 0xbb, 0x62, 0x2c, 0x9b, 0x52, 0x2a, 0xc7, 0x65, 0x8c, 0x6c, 0xdc, 0x7c, 0x44, 0x1e,
    0xb9, 0x19, 0x68, 0xca, 0xb5, 0xa8, 0x4f, 0x26, 0x26, 0xbe, 0xb9, 0xff, 0x4e, 0x9a, 0x85, 0x41,
    0x14, 0x86, 0xbb, 0x96, 0x66, 0x02, 0x5c, 0x59, 0x5e, 0x4d, 0x1f, 0x44, 0x89, 0x80, 0xc0, 0x87,
    0xf5, 0xde, 0xf0, 0x1d, 0x5c, 0x05, 0x14, 0x50, 0x69, 0x3c, 0x9c, 0x31, 0x34, 0x13, 0x65, 0x72,
    0xac, 0x24, 0xd5, 0x3e, 0x33, 0xd5, 0x40, 0xb7, 0x1b, 0xf7, 0x96, 0x8f, 0xfb, 0x59, 0xf1, 0x72,
    0xf4, 0x77, 0x05, 0xf3, 0xa4, 0xad, 0x81, 0x3a, 0x49, 0xe6, 0xfb, 0x58, 0xb7, 0x71, 0x9a, 0x2c,
    0x37, 0x31, 0xa6, 0xc7, 0x6c, 0xea, 0xa2, 0x4a, 0x39, 0x56, 0x64, 0xcb, 0xee, 0x39, 0xac, 0xe0,
    0x30, 0x1c, 0x40, 0x6d, 0xed, 0x95, 0x71, 0x72, 0x5c, 0x32, 0x9e, 0x3a, 0xea, 0x90, 0x77, 0x51
  };

  // Convert vectors to strings
  std::string digest(digestVector.begin(), digestVector.end());
  std::string signature2(signatureVector.begin(), signatureVector.end());

  // Create AsymmetricKey object from public key
  pep::AsymmetricKey publicKey3 = pep::AsymmetricKey(inputPublicKey3);

  // Verify the signature
  EXPECT_TRUE(publicKey3.verifyDigest(digest, signature2));
}

TEST(AsymmetricKeyTest, TestPEM) {
  pep::AsymmetricKey privateKey1 = pep::AsymmetricKey(inputPrivateKey1);
  pep::AsymmetricKey publicKey1 = pep::AsymmetricKey(inputPublicKey1);

  std::string privateKeyPEM = privateKey1.toPem();
  EXPECT_NE(privateKeyPEM, "");
  EXPECT_EQ(privateKeyPEM, inputPrivateKey1);

  std::string publicKeyPEM = publicKey1.toPem();
  EXPECT_NE(publicKeyPEM, "");
  EXPECT_EQ(publicKeyPEM, inputPublicKey1);
}

TEST(AsymmetricKeyPairTest, Generation) {
  pep::AsymmetricKeyPair keyPair;
  keyPair.generateKeyPair();
}

TEST(AsymmetricKeyPairTest, EncryptionDecryption) {
  pep::AsymmetricKeyPair keyPair;
  keyPair.generateKeyPair();

  pep::AsymmetricKey privateKey = keyPair.getPrivateKey();
  pep::AsymmetricKey publicKey = keyPair.getPublicKey();

  std::string testData = "TestData";
  std::string encryptedData = publicKey.encrypt(testData);
  EXPECT_NE(encryptedData, "");

  std::string decryptedData = privateKey.decrypt(encryptedData);
  EXPECT_EQ(testData, decryptedData);
}
