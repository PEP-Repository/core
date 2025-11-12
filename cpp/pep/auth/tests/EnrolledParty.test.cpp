#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/tests/X509Certificate.Samples.test.hpp>
#include <gtest/gtest.h>

TEST(EnrolledParty, NotFromTlsCertificate) {
  auto cert = pep::X509Certificate::FromPem(accessmanagerTLSCertPEM);
  EXPECT_EQ(GetEnrolledParty(cert), pep::EnrolledParty::Unknown);
}
