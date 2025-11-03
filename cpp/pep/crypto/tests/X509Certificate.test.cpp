#include <gtest/gtest.h>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/crypto/tests/X509Certificate.Samples.test.hpp>

#include <openssl/err.h>

#include <string>

using namespace std::literals;

namespace {

const std::string ExpiredLeafCertSignedWithserverCACertPEM = "-----BEGIN CERTIFICATE-----\n\
MIIErzCCApegAwIBAgIRAKTbUN88p6ZnxLLiiOo/8KswDQYJKoZIhvcNAQELBQAw\n\
gaYxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQHDAhO\n\
aWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxJzAlBgNVBAsM\n\
HlBFUCBJbnRlcm1lZGlhdGUgUEVQIFNlcnZlciBDQTEnMCUGA1UEAwweUEVQIElu\n\
dGVybWVkaWF0ZSBQRVAgU2VydmVyIENBMB4XDTI1MDEwMzExMzUxN1oXDTI1MDEw\n\
MzExMzcxN1owKzEYMBYGA1UEAwwPRXhwaXJlZExlYWZDZXJ0MQ8wDQYDVQQLDAZU\n\
ZXN0T1UwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCdlJGa6BKlTENo\n\
gcoBO0VdgeI+7AYViAOVbf8m6uarlw9LRJ3D5vy5Z8NnXlxbDAvNzGlGqIYTQL97\n\
6sKNpnN7DxskRCgbg/UO7V2Xj1SuAA5uvJJR97jb3KHewavAw0PLfFHXXlBqfR0Q\n\
hj096DNDIspEFKLVfWJ3m9ev7qeVLhYYm723JpTlaakQ6MhLjaMGPkFwCSbRykcD\n\
bRZsk46rjR0NOhrVk68By8d2aN1xW0oCDGjhPylgU72+pUkO2/Wls1h7fG291J4N\n\
5ZyIUAT2lKj4/VGd4/XHhES3JKSCVUvcSrYSJVOrNQBiwOJG3mH5Mhu7H6/efBpZ\n\
B5uop+ntAgMBAAGjUjBQMA4GA1UdDwEB/wQEAwIHgDAdBgNVHQ4EFgQUO7cDx5Xn\n\
WsOVYMANVjzUqEm3luwwHwYDVR0jBBgwFoAUkURxKyE/HA4K2UniPBtM8UMi+kAw\n\
DQYJKoZIhvcNAQELBQADggIBAAf+5ZpD5da7QPBvksTxQUgi8bqiYF+JRjtVBoHZ\n\
r8Xf4Hu2PWSTkM1WtdnWMpqEjxJlu4OnrkMdZdwUGtxtmCsvDBx9fUXkSwgFfb5Z\n\
hG1hXEi8KBc42X9l/ofuNRRMlMfFxHGBOIewObGXpcAXW2E90FT7sPyFr9XTwd1m\n\
8n6smuniCvruOU9WES3a+j5t+q8LcAyBi6Rm80sC1mjoSTtD/ae4E7StEJOtb0R/\n\
h70Wd1x70KNGqPX4DC7Xy/BbYdtRBnsT71NiAsLANghnpl1KchFk/lpJ9wXJKDbj\n\
/0zVhRBgCR+rIBuaAo5VkmvXC6srVLfo3ZyYh72muyD3c8XamVYTXoReAz5EZdGU\n\
38fmEK+iH8+PBDT3VPb0AvubULkhlaUd4tkJ6DF4aAPgvZDCyxv1KkNo0RzanWd/\n\
Lyl+TVyTiiHhFY1JOQeL3+L9sud6bWsGOeAwgYovrFMScuC0sfCrqGsLmZSN2vPz\n\
CJx1lxlDT8jZte/Vd2JLzQzlF038ccHKZG1Fd8uI+S1luoXZ9wHVVjZp5kvKU99n\n\
KGJW4uIk1uBSuREkMJolXOA/MP7UL2eXGAqXuabq8n8CddHxgkpGH7/nWBXStwhp\n\
6BtOe9DUAqrIVs/8917zR6MaiIeBbNOFn/Dy9DVreHvq7daJmD1FXltXPMKtX/Pt\n\
dNUq\n\
-----END CERTIFICATE-----\n";

// From TLSAccessManager.chain
const std::string accessmanagerTLSCertPEM = "-----BEGIN CERTIFICATE-----\n\
MIIF6TCCA9GgAwIBAgIUCHsv/XWh8kuqGtdHrQNJp4LHr1kwDQYJKoZIhvcNAQEL\n\
BQAwgZgxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQH\n\
DAhOaWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxIDAeBgNV\n\
BAsMF1BFUCBJbnRlcm1lZGlhdGUgVExTIENBMSAwHgYDVQQDDBdQRVAgSW50ZXJt\n\
ZWRpYXRlIFRMUyBDQTAeFw0yNDEwMzAxNTM2NDlaFw0yNTEwMzAxNTM2NDlaMIGE\n\
MQswCQYDVQQGEwJOTDETMBEGA1UECAwKR2VsZGVybGFuZDERMA8GA1UEBwwITmlq\n\
bWVnZW4xHTAbBgNVBAoMFFJhZGJvdWQgVW5pdmVyc2l0ZWl0MRYwFAYDVQQLDA1B\n\
Y2Nlc3NNYW5hZ2VyMRYwFAYDVQQDDA1BY2Nlc3NNYW5hZ2VyMIIBIjANBgkqhkiG\n\
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAlApk+pwp3ob1reMG8oxOpv7bwitTJtoJ0aqt\n\
qkmxC2+0D6pp4wh9qnDKtLoJkFQKu0WBc0zqUelOsZ9xYvBLP6uy5l35GliZCXXU\n\
KNuaXdEdwjm5xxXvjVtUc4RMF+OoZMbqWHjCNSWnj+xCY6xwW0Ep49iOI8MrMqus\n\
UM23AEVKN3IfWzyGRG6HGMsprfY2sJ7ZcMIjP64Mc7yV8Djm8exw8YTICzNmeKmN\n\
RtgQ9KX4DDsojRihUK0VtMTY83ZHAOtP1xAcLqImER3vp1sxphanL7MjnXOEgT64\n\
M4u22QsjZvqogKd/OFHzMAKaOy4cIpzHXt89fwhldVpi2XGpnwIDAQABo4IBOzCC\n\
ATcwCQYDVR0TBAIwADAjBgNVHREEHDAagglsb2NhbGhvc3SCDUFjY2Vzc01hbmFn\n\
ZXIwHQYDVR0OBBYEFEZGUdnJ1h0l3SSnLZUEnv4RbmMkMIHABgNVHSMEgbgwgbWA\n\
FHH4ZgWD4dUIrO5nMu70vGftr5lqoYGGpIGDMIGAMQswCQYDVQQGEwJOTDETMBEG\n\
A1UECAwKR2VsZGVybGFuZDERMA8GA1UEBwwITmlqbWVnZW4xHTAbBgNVBAoMFFJh\n\
ZGJvdWQgVW5pdmVyc2l0ZWl0MRQwEgYDVQQLDAtQRVAgUm9vdCBDQTEUMBIGA1UE\n\
AwwLUEVQIFJvb3QgQ0GCFAh7L/11ofJLqhrXR60DSaeCx69UMA4GA1UdDwEB/wQE\n\
AwIFoDATBgNVHSUEDDAKBggrBgEFBQcDATANBgkqhkiG9w0BAQsFAAOCAgEAAwlc\n\
SDZceKstkTUhdgrx6dbst7rRrozMoQmS500JDquhxfKKCkyfMSE4ghFZ7z7QYZ/s\n\
fewy7LqqUTOpLPyqx/CM5LFdgD5dqwQfmisX1QLjlKa9e6xguNgB9ErxqpDsHzYZ\n\
jAM9RHsx13MKQ2H8yEXONwVtdWq/WOjd/8i2lZ4vYc15pb1P6ekk/aDuG9qzOrGB\n\
bYDAiEyehX03d4nWwTAYmYjT49lC3s2CoE2NM7j6QZTrahzvKyizb53rgSRD8EXt\n\
E5JcONfXBpmOe2Xpv65e+dYS4u2OWlZp+IFvt1S63WWFZd9h0FEaJxPaDL5933nR\n\
j/NDG1vB6QXKW0hIuFCMmC/eIpeQHOCr0W0GeQws6tyw2sEM2tx0ZPBrGL8zCFHP\n\
AaZem9IfWSc6y/BqTpKLmLRl/QL2N5VcZ9FfrybNJ4s2rD6UYLFX4to7oMwVAmb8\n\
6WbpA1t753GrSFfJkq4KuXFr0qKjNs8hEnhhfJHLcVORMydl+IxmZd52Z/791HOU\n\
l3aN3A/BXly/1qbDYflgMcqh6XlYg7MNjQ58MsDYCnwStP/9DdQzOrcO0s7H4ycj\n\
LDNGrvS2dlr7jcKVsa9dvC77moXLfYT01wxPhL7nnGT2MKazE+VH5UCoCpzTiTH5\n\
QcENulXyum8vAHsbus1YxAneLDnKsw/i92y76lU=\n\
-----END CERTIFICATE-----\n";

const std::string caCertificateDer(reinterpret_cast<char const*>(pepServerCACertDER.data()), pepServerCACertDER.size());

const std::string rootCACertPEMExpired = "-----BEGIN CERTIFICATE-----\n\
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\n\
-----END CERTIFICATE-----\n";

const std::string serverCACertPEMWithExpiredRoot = "-----BEGIN CERTIFICATE-----\n\
MIIGHDCCBASgAwIBAgIUcT+NJ2BEzIwHF7gvg0G/84WPWP4wDQYJKoZIhvcNAQEL\n\
BQAwgYAxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQH\n\
DAhOaWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxFDASBgNV\n\
BAsMC1BFUCBSb290IENBMRQwEgYDVQQDDAtQRVAgUm9vdCBDQTAeFw0yNDExMDEx\n\
NTM2NTlaFw0yNTExMDExNTM2NTlaMIGmMQswCQYDVQQGEwJOTDETMBEGA1UECAwK\n\
R2VsZGVybGFuZDERMA8GA1UEBwwITmlqbWVnZW4xHTAbBgNVBAoMFFJhZGJvdWQg\n\
VW5pdmVyc2l0ZWl0MScwJQYDVQQLDB5QRVAgSW50ZXJtZWRpYXRlIFBFUCBTZXJ2\n\
ZXIgQ0ExJzAlBgNVBAMMHlBFUCBJbnRlcm1lZGlhdGUgUEVQIFNlcnZlciBDQTCC\n\
AiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANBhiT9UG4kLmv+H83mkSklh\n\
dVclB6njGANIZzkU/HXilxvWx0dGhnal0oFO6ddTHe4ow+bUqKfg4kmj6VgjzRBo\n\
R1axYpNVgmkvahe6dpai/I/4Zey0I/6/BbOSOvb5rSFRIP+dO9CqVBgkfA9OtW2S\n\
Cax6Ek4J6+BmVKRty0xjFSHSxwrAZGgYGvrlFFLZRW1g2QTxwKQXcHlgABpmBeCA\n\
wVzZlyTYZQO6oOUihgKHQtYqGeYLzZf6bgsFGxnp+gqHgehrLjCKhgRH30sVIn2Y\n\
igyndQLmul5k7G1zsBu1SyXCIEG50zu7D46g3lAxWmcFg/WpeFptL7TRESzODvrK\n\
jCCRE35Uz8D9xwXlR5tsX1BfmILLzqPvt9+MZ/hluBEyG6xVKhKSmTA7+etHMOVt\n\
OlyaawiO5hzCY9YYLN25zktUFUkGoPR+DldXsrGUZFSsboMHdBEanCiW6hXjwxJw\n\
R8GOgmxbRxfhArztrgJC4DrzYklBoQyqwE8lOJt/ncuUGWDbonvGCG7TfTnyN32t\n\
XdWw1YypibN8lWoAl/qXnfzV8puHarEhP57kwymK3v9H1Sk8VgHej5z7OqVS4oz5\n\
BoIaPUnQXcFw+miZFXWLlpl6Z+vptLQcVw7MUpfdSVky/EaLtfmkncKnzxbOjvxG\n\
XgFlSbaCmJ6ri0lFZOxJAgMBAAGjZjBkMBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYD\n\
VR0OBBYEFP3xKtR+VpoHD7VCtSvsD4EP7Wt5MB8GA1UdIwQYMBaAFBzQ0iy0gwMc\n\
kk2WDQG0ZbwzpBwhMA4GA1UdDwEB/wQEAwIBhjANBgkqhkiG9w0BAQsFAAOCAgEA\n\
MjD4FIuNx8nTSboXzK8YpxIzWTfisyk/jUGsxBgu8NygKSOm3eXQfKieK72gfEWT\n\
/iOuS/hye9AStsVse16E9M6aFHMrtMk1uzbh6qLJ4U3oGv1GtJhzO1O/X7Gh30nt\n\
cWPPreSQYlIIeJcGk78Hr9jd0qV/gN/Qk1Eq7PLsMLqZ//vldERrNrxjsptST509\n\
+rU+XWj035JpWCqoH4SyUxVITFdxwYmKxuh0So0OCgE1GV/PXHjvBsJMSzYiRwnl\n\
BUQFusxd6IET77ZForUVQYUaGz1YI6bM+1kfuDM5lQJgDphsI2+sn5xM9DUcVXkV\n\
Dvpy+Wb2xpfWGwofQsfzYMAfWRCorYKgaZXWWpZYHQLs1zEQ3WESj7w987UI0Jx7\n\
C8NM1K6W3wd5Ay2bfZ1mLCw0EVf0gqlXMfX3/ZBcLJSUCptKNJgvGxi9AJ9+NwiG\n\
khF3WRNBy+o0BWIuHTLv0iupSEEj9umrTFuoQhjuqOY71HD676ItEKC3ygReEvii\n\
vMxDtXq1kRgDLTt1J/ja8hwZAg2duphKIz7TvMFp7844ygv9Pcdz35Rse9UEV25H\n\
6wAdjnyRh+cGvnFFDPUJOKD++iFS7c3p6pff8rt1Bgg1noBC7tqlvZCR0TSdg3HT\n\
9znrMhSgJqESVVj4uH/wuKLEIcsI9BoQVtjJKkX6lhM=\n\
-----END CERTIFICATE-----\n";

const std::string authserverCertPEMWithExpiredRoot = "-----BEGIN CERTIFICATE-----\n\
MIIFEDCCAvigAwIBAgIUcT+NJ2BEzIwHF7gvg0G/84WPWQswDQYJKoZIhvcNAQEL\n\
BQAwgaYxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQH\n\
DAhOaWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxJzAlBgNV\n\
BAsMHlBFUCBJbnRlcm1lZGlhdGUgUEVQIFNlcnZlciBDQTEnMCUGA1UEAwweUEVQ\n\
IEludGVybWVkaWF0ZSBQRVAgU2VydmVyIENBMB4XDTI0MTEwMTE1MzcwMFoXDTI1\n\
MTEwMTE1MzcwMFowfjELMAkGA1UEBhMCTkwxEzARBgNVBAgMCkdlbGRlcmxhbmQx\n\
ETAPBgNVBAcMCE5pam1lZ2VuMR0wGwYDVQQKDBRSYWRib3VkIFVuaXZlcnNpdGVp\n\
dDETMBEGA1UECwwKQXV0aHNlcnZlcjETMBEGA1UEAwwKQXV0aHNlcnZlcjCCASIw\n\
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJ8ykw17O4BI1KO0s0mfT+vmdvxH\n\
rlzU87PMSRS9lFLZtW6QkVCGtWwaHJ6ug6Mj5c4lpXuUxqUvtVX9ffUnL1xpMpBL\n\
+qS30I4NYnXMyDrGq8QXf/B3xdMBGLL0+PetVoWMeNBRD8pZlEZM9Xjjz0tA6Xym\n\
oEKMddobDLHMNqWLBRa+FMvw70tuUb8g0I+2pVv3ayTpdcQLsnc3bBlrI9nh8AVD\n\
c9ReoXqPzm3AscSqVuOPCEYGKyDtU6AqXPSOte0Kr9at4oYUOAeexWQC2y03eqsH\n\
iLS+5n/cwfKsGqO1DWX/i4V06q1CfsOiTY2NdtpyVEwafIY54KhBjRNFqRUCAwEA\n\
AaNdMFswCQYDVR0TBAIwADAdBgNVHQ4EFgQUYk+4ZudmcBklWMcliZJL3SlOEC8w\n\
HwYDVR0jBBgwFoAU/fEq1H5WmgcPtUK1K+wPgQ/ta3kwDgYDVR0PAQH/BAQDAgbA\n\
MA0GCSqGSIb3DQEBCwUAA4ICAQBRitrVwFkY/VVkoJpbHGUkppuC2/UmHuox0kg9\n\
HLyHXz4H3bKI3+1hqRaC6/EII95vaQxXIPPxm9u5dqNTh1g8DheBYsLxOhTJoU9I\n\
LxlN80CuNWDASisMz3uL92FKMmF1bWlcWJekIVnnkfxRutiffzYl+t7qclUw+lpf\n\
qgYmSY8jCPnks1atuuCc1c+wdGlRZP7shzrdvaC+YFG1reRVXoEUZI6pmxtpx/Pf\n\
KnjZ+pc5liTZ6XuTInCyf6xLq333I6m0TezzhlJdWDLP87FMLdNUguOP9tpx9ToZ\n\
MLc6HubEvoBNBiSes/yBp7zVZaVVY0LrWvom82DTcFhRhN6Vu1YHHuCDluHRQByE\n\
DTcA9u0jGoN/MhugVIxegW/chY3aDt/vVxtuTDxLIj0HZiBCF5EKj29W5tNgVW7M\n\
bNErLBgLkV0nuOwz/ylnrxWFF3OBliRPvl0O0U5P6bJxNiom6JetPcdng9TU3K4+\n\
P6nO/shthDl73LnjysCWQ5k7Ft2p3VsqnPEdUT6GqT26TnGyHKjUz3fe7P9e9dUl\n\
BLIY1CF83Tzd/vLHMAyanlH5CtcKiTIhuGSp47+sSwEbIHDE88OJtdwHda9713sL\n\
JoduuYQJcUrdusLDlfL9XHkxi8DFQIb1Pakz5Ndj+4Y5hQcau5YgC/jRouitSre5\n\
UfrRCw==\n\
-----END CERTIFICATE-----\n";



TEST(X509CertificateTest, CopyConstructor) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate caCertificate2(caCertificate);
  EXPECT_EQ(caCertificate.toPem(), caCertificate2.toPem());
}

TEST(X509CertificateTest, AssignmentOperator) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate caCertificate2;
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

TEST(X509CertificateTest, IsntServerCertificate) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(pepServerCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(pepServerCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, 1min);

  EXPECT_FALSE(cert.isPEPServerCertificate()) << "Certificate is incorrectly identified as a server certificate";
}

TEST(X509CertificateTest, IsServerCertificate) {

  pep::X509Certificate cert = pep::X509Certificate::FromPem(accessmanagerTLSCertPEM);

  EXPECT_TRUE(cert.isPEPServerCertificate()) << "Certificate is not a server certificate.";
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
  pep::X509Certificate expiredCert = pep::X509Certificate::FromPem(ExpiredLeafCertSignedWithserverCACertPEM);

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
  EXPECT_EQ(cert.toDer(), caCertificateDer);
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
  ASSERT_FALSE(cert.getPathLength().has_value()) << "Generated certificate has a pathlength constraint";

  // The intermediate root certificate should however have basic constraints and a pathlength of 0
  pep::X509Certificate intermediateCACert = pep::X509Certificate::FromPem(pepServerCACertPEM);
  ASSERT_TRUE(intermediateCACert.hasBasicConstraints()) << "Intermediate CA cert does not have Basic Constraints";
  ASSERT_TRUE(intermediateCACert.getPathLength().has_value()) << "Intermediate CA cert does not have a pathlength constraint";
  ASSERT_TRUE(intermediateCACert.getPathLength().value() == 0) << "Intermediate CA cert has a pathlength constraint different from 0";
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
  EXPECT_THROW(pep::X509Certificates certificates(""), std::runtime_error);

  // Certificates chains in PEM format can be interleaved with text, for example as comments
  EXPECT_NO_THROW(pep::X509Certificates certificates("extra text\n" + pepAuthserverCertPEM + pepServerCACertPEM));
  EXPECT_NO_THROW(pep::X509Certificates certificates(pepAuthserverCertPEM + "extra text\n" + pepServerCACertPEM));

  // But bad formatting after a -----BEGIN CERTIFICATE----- block should produce an error
  EXPECT_THROW(pep::X509Certificates certificates(pepAuthserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting\n-----END CERTIFICATE-----" + pepServerCACertPEM), std::runtime_error);
  // Also without a -----END CERTIFICATE----- block
  EXPECT_THROW(pep::X509Certificates certificates(pepAuthserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting" + pepServerCACertPEM), std::runtime_error);
  // But bad formatting with only an -----END CERTIFICATE----- doesnt throw an error
  EXPECT_NO_THROW(pep::X509Certificates certificates(pepAuthserverCertPEM + "bad formatting\n-----END CERTIFICATE-----\n" + pepServerCACertPEM));

  // Openssl error should be cleared after parsing errors
  EXPECT_TRUE(ERR_get_error() == 0) << "Openssl errors are not cleared after parsing errors";

  // If the previous test fails, still make sure the rest of the tests have the correct error queue state, if it didn't fail the error queue is empty anyway
  ERR_clear_error();
}

TEST(X509CertificatesTest, ToPem) {
  pep::X509Certificates certificates(pepAuthserverCertPEM + pepServerCACertPEM);
  std::string expectedPem = pepAuthserverCertPEM + pepServerCACertPEM;
  EXPECT_EQ(certificates.toPem(), expectedPem) << "PEM conversion of X509Certificates failed";
}

TEST(X509CertificateChainTest, VerifyCertificateChain) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEM);
  ASSERT_TRUE(rootCA.front().isCurrentTimeInValidityPeriod());

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(pepAuthserverCertPEM + pepServerCACertPEM);
  ASSERT_TRUE(certChain.isCurrentTimeInValidityPeriod());

  // Verify the certificate chain against the root CAs
  EXPECT_TRUE(certChain.verify(rootCA)) << "Certificate chain verification failed";
}

TEST(X509CertificateChainTest, VerifyCertificateChainWithExpiredRootCA) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEMExpired);

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(authserverCertPEMWithExpiredRoot + serverCACertPEMWithExpiredRoot);

  // Verify the certificate chain against the root CAs
  EXPECT_FALSE(certChain.verify(rootCA)) << "Certificate chain verification succeeded with expired root CA";
}

TEST(X509CertificateChainTest, VerifyCertificateChainWithExpiredLeafCert) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEM);

  // Create the certificate chain with the expired leaf certificate and the CA certificate
  pep::X509CertificateChain certChain(ExpiredLeafCertSignedWithserverCACertPEM + pepServerCACertPEM);

  // Verify the certificate chain against the root CAs
  EXPECT_FALSE(certChain.verify(rootCA)) << "Certificate chain verification succeeded with expired leaf certificate";
}

TEST(X509CertificateChainTest, VerifyCertificateChainOrdering) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEM);
  ASSERT_TRUE(rootCA.front().isCurrentTimeInValidityPeriod());

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(pepServerCACertPEM + pepAuthserverCertPEM);
  ASSERT_TRUE(certChain.isCurrentTimeInValidityPeriod());

  // Verify the certificate chain against the root CAs
  EXPECT_TRUE(certChain.verify(rootCA)) << "Certificate chain verification failed for reverse ordering";
}

TEST(X509CertificateChainTest, CertifiesPrivateKey) {
  pep::X509CertificateChain certChain(pepServerCACertPEM + rootCACertPEM);
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
