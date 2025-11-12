#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/tests/X509Certificate.Samples.test.hpp>
#include <gtest/gtest.h>

using namespace std::literals;

TEST(EnrolledParty, NotFromTlsCertificate) {
  auto cert = pep::X509Certificate::FromPem(accessmanagerTLSCertPEM);
  EXPECT_FALSE(GetEnrolledParty(cert).has_value());
}

TEST(EnrolledParty, IsntServerCertificate) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_FALSE(pep::IsServerSigningCertificate(cert)) << "Certificate is incorrectly identified as a server certificate";
}
