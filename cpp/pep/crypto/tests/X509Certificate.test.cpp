#include <gtest/gtest.h>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/crypto/tests/X509Certificate.Samples.test.hpp>

#include <openssl/err.h>

#include <string>

using namespace std::literals;

namespace {

TEST(X509CertificateTest, CopyConstructor) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate caCertificate2(caCertificate);
  EXPECT_EQ(caCertificate.toPem(), caCertificate2.toPem());
}

TEST(X509CertificateTest, AssignmentOperator) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate caCertificate2 = pep::X509Certificate::FromPem(rootCACertPEM);
  EXPECT_NE(caCertificate.toPem(), caCertificate2.toPem());

  caCertificate2 = caCertificate;
  EXPECT_EQ(caCertificate.toPem(), caCertificate2.toPem());
}

TEST(X509CertificateTest, GetPublicKey) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::AsymmetricKey publicKey = caCertificate.getPublicKey();
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  EXPECT_TRUE(caPrivateKey.isPrivateKeyFor(publicKey));
}

TEST(X509CertificateTest, GetCommonName) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_EQ(cert.getCommonName(), testCN) << "CN in certificate does not match expected value";
}

TEST(X509CertificateTest, GetOrganizationalUnit) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_EQ(cert.getOrganizationalUnit(), testOU) << "OU in certificate does not match expected value";
}

TEST(X509CertificateTest, GetIssuerCommonName) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_EQ(cert.getIssuerCommonName(), "PEP Intermediate PEP Server CA") << "Issuer CN in certificate does not match expected value";
}

TEST(X509CertificateTest, DoesntHaveTLSServerEKU) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_FALSE(cert.hasTLSServerEKU()) << "Certificate unexpectedly has a TLS Server EKU";
}

TEST(X509CertificateTest, HasTLSServerEKU) {
  pep::X509Certificate cert = pep::X509Certificate::FromPem(accessmanagerTLSCertPEM);

  EXPECT_TRUE(cert.hasTLSServerEKU()) << "Certificate doesn't have a TLS Server EKU.";
}

TEST(X509CertificateTest, CertificateValidity) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);

  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::X509CertificateSigningRequest csr2(keyPair, testCN, testOU);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);
  pep::X509Certificate cert2 = csr2.signCertificate(caCertificate, caPrivateKey, 1h);
  pep::X509Certificate expiredCert = pep::X509Certificate::FromPem(pepAuthserverCertPEMExpired);

  EXPECT_TRUE(cert.isCurrentTimeInValidityPeriod()) << "Certificate should be within the validity period";
  EXPECT_TRUE(cert2.isCurrentTimeInValidityPeriod()) << "Certificate should be within the validity period";

  EXPECT_FALSE(expiredCert.isCurrentTimeInValidityPeriod()) << "Certificate should not be within the validity period";
}

TEST(X509CertificateTest, ToPEM) {
  pep::X509Certificate cert = pep::X509Certificate::FromPem(pepServerCACertPEM);
  EXPECT_EQ(cert.toPem(), pepServerCACertPEM);
}

TEST(X509CertificateTest, ToDER) {
  pep::X509Certificate cert = pep::X509Certificate::FromPem(pepServerCACertPEM);
  EXPECT_EQ(cert.toDer(), pep::SpanToString(pepServerCACertDER));
}

TEST(X509CertificateTest, SelfSigned) {
  auto cert = pep::X509Certificate::MakeSelfSigned(pep::AsymmetricKeyPair::GenerateKeyPair(), "Metacortex", "Mr. Anderson", "US");
  EXPECT_TRUE(cert.isSelfSigned());

  auto chain = pep::X509CertificateChain({ std::move(cert) });
  auto rootCAs = pep::X509RootCertificates({ chain.leaf() });
  EXPECT_TRUE(chain.verify(rootCAs));
}

TEST(X509CertificateSigningRequestTest, GenerationAndSigning) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  EXPECT_EQ(csr.getCommonName(), testCN) << "CN in CSR does not match input";
  EXPECT_EQ(csr.getOrganizationalUnit(), testOU) << "OU in CSR does not match input";

  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_EQ(cert.getCommonName(), testCN) << "CN in certificate does not match input";
  EXPECT_EQ(cert.getOrganizationalUnit(), testOU) << "OU in certificate does not match input";
}

TEST(X509CertificateSigningRequestTest, CertificateDuration) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);

  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  auto invalidMaximumDuration = std::chrono::years{2} + 1s; // Duration above 2 years
  auto invalidMinimumDuration = -1s;

  EXPECT_THROW(csr.signCertificate(caCertificate, caPrivateKey, invalidMinimumDuration), std::invalid_argument) << "Signing a certificate with a negative duration did not throw an error";
  EXPECT_THROW(csr.signCertificate(caCertificate, caPrivateKey, invalidMaximumDuration), std::invalid_argument) << "Signing a certificate with a duration above 2 years did not throw an error";
}

TEST(X509CertificateSigningRequestTest, VerifySignature) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  EXPECT_TRUE(csr.verifySignature()) << "Signature verification of correct signature threw an error";

  pep::proto::X509CertificateSigningRequest proto;
  pep::Serializer<pep::X509CertificateSigningRequest> serializer;
  serializer.moveIntoProtocolBuffer(proto, csr);

  csr = serializer.fromProtocolBuffer(std::move(proto));
  EXPECT_TRUE(csr.verifySignature()) << "Signature verification of correct signature after proto roundtrip threw an error";

  serializer.moveIntoProtocolBuffer(proto, csr);
  auto der = proto.mutable_data();
  der->back() = static_cast<char>(static_cast<unsigned char>(der->back()) ^ 0xff);

  csr = serializer.fromProtocolBuffer(std::move(proto));
  EXPECT_FALSE(csr.verifySignature()) << "Signature verification of invalid signature did not throw an error";

  // Openssl errors should be cleared after parsing errors, a false result with verification will still produce an error
  EXPECT_TRUE(ERR_get_error() == 0) << "Openssl errors are not cleared after parsing errors";

  // If the previous test fails, still make sure the rest of the tests have the correct error queue state, if it didn't fail the error queue is empty anyway
  ERR_clear_error();
}

TEST(X509CertificateSigningRequestTest, CertificateExtensions) {

  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  // The generated certificate should have the following extensions set:
  ASSERT_TRUE(cert.hasDigitalSignatureKeyUsage()) << "Generated certificate does not have Digital Signature Key Usage";

  // Warning: We are assuming that the KI extension is a SHA-1 hash of the public key,
  // which is not by RFC definition always the case. Openssl may change this behavior in the future, breaking our test.
  // In that case re-evaluate the testing of the certificate extensions or fix the VerifyKeyIdentifier function in X509Certificate.cpp.
  ASSERT_TRUE(cert.verifySubjectKeyIdentifier()) << "Generated certificate does not have a valid Subject Key Identifier";
  ASSERT_TRUE(cert.verifyAuthorityKeyIdentifier(caCertificate)) << "Generated certificate does not have a valid Authority Key Identifier";

  // The generated certificate should not have basic constraints set
  ASSERT_FALSE(cert.hasBasicConstraints()) << "Generated certificate has the Basic Constraints set, which it should not have";
  // And it should not have a path length constraint, so the result should be std::nullopt
  ASSERT_FALSE(cert.pathLengthConstraint().has_value()) << "Generated certificate has a pathlength constraint";

  // The intermediate root certificate should however have basic constraints and a pathlength of 0
  pep::X509Certificate intermediateCACert = pep::X509Certificate::FromPem(pepServerCACertPEM);
  ASSERT_TRUE(intermediateCACert.hasBasicConstraints()) << "Intermediate CA cert does not have Basic Constraints";
  ASSERT_TRUE(intermediateCACert.pathLengthConstraint().has_value()) << "Intermediate CA cert does not have a pathlength constraint";
  ASSERT_TRUE(intermediateCACert.pathLengthConstraint().value() == 0) << "Intermediate CA cert has a pathlength constraint different from 0";
}

TEST(X509CertificateSigningRequestTest, UTF8CharsInUTFField) {

  std::string UTF8TestCN = std::string("Тестовая строка"); // UTF-8 string in Russian

  std::string UTF8TestOU = std::string("Ć̶̨t̶̪̊h̸̠͒ȗ̸̘l̵͙̇h̶̥̑u̵͍̓ ̴̖̿r̸̹͒i̷̩̍s̸̘̅e̵̝͒s̶͇̓"); // He comes

  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();

  // Create CSR with UTF-8 string in CN field
  pep::X509CertificateSigningRequest csr(keyPair, UTF8TestCN, UTF8TestOU);

  // Verify that the CN field contains the UTF-8 string
  EXPECT_EQ(csr.getCommonName(), UTF8TestCN) << "CN in CSR does not match UTF-8 input";
  EXPECT_EQ(csr.getOrganizationalUnit(), UTF8TestOU) << "OU in CSR does not match UTF-8 input";

  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);

  // Sign the certificate
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  // Verify that the fields in the certificate contain the UTF-8 string
  EXPECT_EQ(cert.getCommonName(), UTF8TestCN) << "CN in certificate does not match UTF-8 input";
  EXPECT_EQ(cert.getOrganizationalUnit(), UTF8TestOU) << "OU in certificate does not match UTF-8 input";
}

// As per X.509 ASN.1 specification (https://www.rfc-editor.org/rfc/rfc5280#appendix-A):
// the CN and OU are limited to 64 characters (64 code points if using UTF8String)
TEST(X509CertificateSigningRequestTest, LongStringInField) {
  std::string testCNSucceeds = std::string(64, 'A');
  std::string testCNFails = std::string(65, 'A');
  std::string testOUSucceeds = std::string(64, 'A');
  std::string testOUFails = std::string(65, 'A');

  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();

  // Create CSR that should work
  EXPECT_NO_THROW(pep::X509CertificateSigningRequest csr(keyPair, testCNSucceeds, testOUSucceeds)) << "Creating a CSR fails with valid strings as CN and OU";

  // Create CSR with a very long string in the CN field
  EXPECT_ANY_THROW(pep::X509CertificateSigningRequest csr(keyPair, testCNFails, testOUSucceeds)) << "Creating a CSR with a too long CN string does not throw an error";
  // Create CSR with a very long string in the OU field
  EXPECT_ANY_THROW(pep::X509CertificateSigningRequest csr(keyPair, testCNSucceeds, testOUFails)) << "Creating a CSR with a too long OU string does not throw an error";
}

TEST(X509CertificatesTest, X509CertificatesFormatting) {

  // An empty string input should throw an error
  EXPECT_THROW(pep::X509CertificatesFromPem(""), std::runtime_error);

  // Certificates chains in PEM format can be interleaved with text, for example as comments
  EXPECT_NO_THROW(pep::X509CertificatesFromPem("extra text\n" + pepAuthserverCertPEM + pepServerCACertPEM));
  EXPECT_NO_THROW(pep::X509CertificatesFromPem(pepAuthserverCertPEM + "extra text\n" + pepServerCACertPEM));

  // But bad formatting after a -----BEGIN CERTIFICATE----- block should produce an error
  EXPECT_THROW(pep::X509CertificatesFromPem(pepAuthserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting\n-----END CERTIFICATE-----" + pepServerCACertPEM), std::runtime_error);
  // Also without a -----END CERTIFICATE----- block
  EXPECT_THROW(pep::X509CertificatesFromPem(pepAuthserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting" + pepServerCACertPEM), std::runtime_error);
  // But bad formatting with only an -----END CERTIFICATE----- doesnt throw an error
  EXPECT_NO_THROW(pep::X509CertificatesFromPem(pepAuthserverCertPEM + "bad formatting\n-----END CERTIFICATE-----\n" + pepServerCACertPEM));

  // Openssl error should be cleared after parsing errors
  EXPECT_TRUE(ERR_get_error() == 0) << "Openssl errors are not cleared after parsing errors";

  // If the previous test fails, still make sure the rest of the tests have the correct error queue state, if it didn't fail the error queue is empty anyway
  ERR_clear_error();
}

TEST(X509CertificatesTest, ToPem) {
  pep::X509Certificates certificates = pep::X509CertificatesFromPem(pepAuthserverCertPEM + pepServerCACertPEM);
  std::string expectedPem = pepAuthserverCertPEM + pepServerCACertPEM;
  EXPECT_EQ(X509CertificatesToPem(certificates), expectedPem) << "PEM conversion of X509Certificates failed";
}

TEST(X509CertificateChainTest, VerifyCertificateChain) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(pep::X509CertificatesFromPem(rootCACertPEM));
  ASSERT_TRUE(rootCA.items().front().isCurrentTimeInValidityPeriod());

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(pep::X509CertificatesFromPem(pepAuthserverCertPEM + pepServerCACertPEM));
  ASSERT_TRUE(certChain.isCurrentTimeInValidityPeriod());

  // Verify the certificate chain against the root CAs
  EXPECT_TRUE(certChain.verify(rootCA)) << "Certificate chain verification failed";
}

TEST(X509CertificateChainTest, VerifyCertificateChainWithExpiredRootCA) {
  // Load the root CA certificate
  EXPECT_ANY_THROW(pep::X509RootCertificates(pep::X509CertificatesFromPem(rootCACertPEMExpired)));
}

TEST(X509CertificateChainTest, VerifyCertificateChainWithExpiredLeafCert) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(pep::X509CertificatesFromPem(rootCACertPEM));
  ASSERT_TRUE(rootCA.items().front().isCurrentTimeInValidityPeriod());

  // Create the certificate chain with the expired leaf certificate and the CA certificate
  pep::X509CertificateChain certChain(pep::X509CertificatesFromPem(pepAuthserverCertPEMExpired + pepServerCACertPEM));

  // Verify the certificate chain against the root CAs
  EXPECT_FALSE(certChain.verify(rootCA)) << "Certificate chain verification succeeded with expired leaf certificate";
}

TEST(X509CertificateChainTest, VerifyCertificateChainOrdering) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(pep::X509CertificatesFromPem(rootCACertPEM));
  ASSERT_TRUE(rootCA.items().front().isCurrentTimeInValidityPeriod());

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(pep::X509CertificatesFromPem(pepServerCACertPEM + pepAuthserverCertPEM));
  ASSERT_TRUE(certChain.isCurrentTimeInValidityPeriod());

  // Verify the certificate chain against the root CAs
  EXPECT_TRUE(certChain.verify(rootCA)) << "Certificate chain verification failed for reverse ordering";
}

TEST(X509CertificateChainTest, CertifiesPrivateKey) {
  pep::X509CertificateChain certChain(pep::X509CertificatesFromPem(pepServerCACertPEM + rootCACertPEM));
  pep::AsymmetricKey privateKey(pepServerCAPrivateKeyPEM);
  EXPECT_TRUE(certChain.certifiesPrivateKey(privateKey)) << "Certificate chain does not certify the private key";
}

TEST(X509CertificateSigningRequestTest, GetPublicKey) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey publicKey = csr.getPublicKey();
  EXPECT_TRUE(keyPair.getPrivateKey().isPrivateKeyFor(publicKey)) << "Public key in CSR does not match the private key";
}

TEST(X509CertificateSigningRequestTest, ToPem) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  std::string pem = csr.toPem();
  pep::X509CertificateSigningRequest csrFromPem = pep::X509CertificateSigningRequest::FromPem(pem);
  EXPECT_EQ(csr.toPem(), csrFromPem.toPem()) << "PEM conversion of X509CertificateSigningRequest failed";
}

}
