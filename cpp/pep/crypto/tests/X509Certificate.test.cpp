#include <gtest/gtest.h>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

#include <openssl/err.h>

#include <string>

namespace {

const std::string rootCACertPEM = "-----BEGIN CERTIFICATE-----\n\
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\n\
-----END CERTIFICATE-----\n";

// Created as follows:
// openssl pkey -in pepServerCA.key -out pepServerCA_decrypted.key -passin file:pepServerCA.password
const std::string serverCAPrivateKeyPEM = "-----BEGIN PRIVATE KEY-----\n\
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\n\
Td4gTTcIX7RX7MP7wnszke7Larguyg==\n\
-----END PRIVATE KEY-----\n";

const std::string serverCACertPEM = "-----BEGIN CERTIFICATE-----\n\
MIIGHDCCBASgAwIBAgIUCHsv/XWh8kuqGtdHrQNJp4LHr1UwDQYJKoZIhvcNAQEL\n\
BQAwgYAxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQH\n\
DAhOaWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxFDASBgNV\n\
BAsMC1BFUCBSb290IENBMRQwEgYDVQQDDAtQRVAgUm9vdCBDQTAeFw0yNDEwMzAx\n\
NTM2NDlaFw0yNTEwMzAxNTM2NDlaMIGmMQswCQYDVQQGEwJOTDETMBEGA1UECAwK\n\
R2VsZGVybGFuZDERMA8GA1UEBwwITmlqbWVnZW4xHTAbBgNVBAoMFFJhZGJvdWQg\n\
VW5pdmVyc2l0ZWl0MScwJQYDVQQLDB5QRVAgSW50ZXJtZWRpYXRlIFBFUCBTZXJ2\n\
ZXIgQ0ExJzAlBgNVBAMMHlBFUCBJbnRlcm1lZGlhdGUgUEVQIFNlcnZlciBDQTCC\n\
AiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAKfF+Lvy3SxjScqZ/WZuag9y\n\
nS9noYWrnn55X9ddvpG+pKwCmyiz9oJFHtW+gDqQ9oL1o8gshHsX3HVuKAamBvdE\n\
nHGp2lKfE+kF2gN/NXBFZVEgp5vr4rYojZsmQYOvs1xsy9eGsUvDRSh+clcNzpki\n\
QAsTaLffsPZb8mYgtli51mOrvmFqkQaM0ZztOc6gXmzD5Tf49klEGhmfUQd0JR47\n\
DRFfd5oPa/p33xAZdNIBAVc+HQx5wRaTrX8Q9qWBPxL/QiGBeEb1UdNoXRuVm/Ig\n\
fYajrmHIVOQSD60/3lRHgr+XW7/upcHbvSdelBMQjoQxvbWG3feHu6fseEoXALS3\n\
1FRyguRtHKhYJ0uztu6cGmy1Ep0Xv53IXZ4+jF82U7X17LSL2zH3l3dWed/SFtjY\n\
cKWGljlKKHMrz8zDGbVn9tmCxFiPuGAbjd6l10UWizFh+XjVBkDcOhGe1x1KP4Nu\n\
910zRT9DFeabM7ETG2d2frUKCLGkU4Iihk5plok7M/68lBojBHjjFkOX5Y+E4nk7\n\
IN1vegqhzgS4zaAIhNH7aKh7xYWgWgfX++NzwVLlT6aYsCmeBuEcTmztCeCuMuOI\n\
4mc0xa5zFbDR1FftIYmIrh50rq+/rnn0VcGOhMRdtdq4hfFMWF2g573o2now3aHM\n\
iS5HEXkOPNZ1xXhsx1lzAgMBAAGjZjBkMBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYD\n\
VR0OBBYEFJFEcSshPxwOCtlJ4jwbTPFDIvpAMB8GA1UdIwQYMBaAFPVC2s187G+s\n\
xKYoj51fOFHcaqY7MA4GA1UdDwEB/wQEAwIBhjANBgkqhkiG9w0BAQsFAAOCAgEA\n\
ZD30/R/w7/S8iWMLlyKYQVEdry/6zsLWQfjDMzbKjJvaUMd3uwgz6+u/p3r3Dnbw\n\
6pKMXP8r+LxMIbeci5XQ6s5jR+WlbqaEoISrx+nbR35Ve7Z//auyrDEEYP110Ddf\n\
u/YbkkZAQumY5h/QBabYJwuNplTsnyV/l6xd3/nH35HQWKGOWGnEi7aZrdbsmHfu\n\
B4UUc5zf6JTRYfXK17GDe1GYBiKPEqHSC+3oNmo1+b6LgDh4MPXREKbdjtI8/RJv\n\
NmxDGS9XjZUQA5RS7z1apW2ubn+T6Ry6NqWeQkO/BTeGQXJnpSciv5OuJc8COotH\n\
PDHe0tJhR/vISJHc5/XDFibPwBZq3zXmjBl+tnlYBxHfEXaIG9Q2ne+JDW9A9oXz\n\
mPbb5HgV+TohH2w6uxjv6SSj2ZAt0sGsXEMNHKpR3MLcLhqKsO3mtBT/YfQoJMtB\n\
f2xyu2lrpnP1Acjc7kKDQAFu7h/ZYspWacrdd25nGTbfpVEuIwVJgJcds9Svp9Xq\n\
yzZZwsgOW2OawusTmm8dEau6+X6PLYHdvwplLsz2UnHtDynWKAVegmSTQkAe4Wii\n\
iBzoOCmEUoiGATltjS1EdKLdEZ4cSHceJwHemumSR/rDwYJuObI+ZJroMQ9pICpn\n\
vL6HEF2uCcVTBcJ+WTmZCz8JE77Uvmxv3VkJdyyUG1c=\n\
-----END CERTIFICATE-----\n";

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

const std::string authserverCertPEM = "-----BEGIN CERTIFICATE-----\n\
MIIFEDCCAvigAwIBAgIUCHsv/XWh8kuqGtdHrQNJp4LHr2IwDQYJKoZIhvcNAQEL\n\
BQAwgaYxCzAJBgNVBAYTAk5MMRMwEQYDVQQIDApHZWxkZXJsYW5kMREwDwYDVQQH\n\
DAhOaWptZWdlbjEdMBsGA1UECgwUUmFkYm91ZCBVbml2ZXJzaXRlaXQxJzAlBgNV\n\
BAsMHlBFUCBJbnRlcm1lZGlhdGUgUEVQIFNlcnZlciBDQTEnMCUGA1UEAwweUEVQ\n\
IEludGVybWVkaWF0ZSBQRVAgU2VydmVyIENBMB4XDTI0MTAzMDE1MzY1MFoXDTI1\n\
MTAzMDE1MzY1MFowfjELMAkGA1UEBhMCTkwxEzARBgNVBAgMCkdlbGRlcmxhbmQx\n\
ETAPBgNVBAcMCE5pam1lZ2VuMR0wGwYDVQQKDBRSYWRib3VkIFVuaXZlcnNpdGVp\n\
dDETMBEGA1UECwwKQXV0aHNlcnZlcjETMBEGA1UEAwwKQXV0aHNlcnZlcjCCASIw\n\
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL/ntzjpLYltEnmF8YVqFC2tqiPV\n\
ZH4qhTGXWRKLzjyqYwcFzq9qh740YdUwaz7zY/0S5vR4y8AwGa+yBWmDPdDeNHoE\n\
kOoC/1B7OGQztwYI32/g2K4Rv/4EHrhhX87JN/vlfezFOr/bjhwHgIj1hLvO5FpW\n\
wZeDybadgPTMLG+//IBYDq8jCe6jd22EHLXUjJGsew1/8z2TtGP/LCS37axiHT7x\n\
Bep8gJIk50lt/qchIFlys0BzOtKqV0XS/CQl6hqGV7ObW3W+sOYmoDxA3RHO01Xb\n\
uzyGD/z7xI7XCgRDO1L/fxoqsBLTutnTX5B8WOnCqy2io1Wfs/r19HdW/gUCAwEA\n\
AaNdMFswCQYDVR0TBAIwADAdBgNVHQ4EFgQUMEtoSVgF5w26gJC5UDsjpU0B0AYw\n\
HwYDVR0jBBgwFoAUkURxKyE/HA4K2UniPBtM8UMi+kAwDgYDVR0PAQH/BAQDAgbA\n\
MA0GCSqGSIb3DQEBCwUAA4ICAQBvBEY1kJ1zUvfqbposHzlCcMS02XkgdUvJ0h9I\n\
9STWY9Rk0A/wVpTabuLi2QVw/0v9dkJP5PmFsgIo7yuiViBZhoJTu2ZZWo5Kygha\n\
82s5hSSItBtM4XUuQCIPgB7xw/23QYnXcWIfzL2k8yYRH8qBQGvLhlco9/DduYO8\n\
AASZN8U9PMkfjuQQB+Zb1FWRLA7QPRzePct4KetwRLyBzxbzifGuIFov0GzasGeW\n\
iq7NSsni/m1VedPPeceEgoGayA83T6f5FGMptNtYL6smC0NgDEqDY4AK53GhRurf\n\
iljKbqL5n7Xp6ESBp1Q+2VZCj9bL3Jc5e/TA76QwfpTVlEw2r20gwYOeFHd0C6qZ\n\
YubvllGocOP3e+pm/iYopGg/ztPXZ2d+GFjdfJwzB2V2KlQs0NhTDQxf0OuVIUos\n\
lA6TmxKiuqE7EH9p/iGLAt4sECjB8SC7rSrRbaGDAeIylQvG9U2pEpAV/0YelkWi\n\
h768LFHPoEQZ7jIAh3ePXFqOne52M1AQ7ATH2nIYPvP0ZLRAw68mVo1LnpGHaBbn\n\
q0TD5eXHRcoL7X9Zt2DQUUGXj1AsAkIps5qlA6EhFWEXIDl6C0FLgUdD7Wn+3d55\n\
LcdGYygZi455BDa+C1DMGSdstmpXn9BD+PzaYiyDxAHZIIOvinOz8Qpv3j6/Y2S9\n\
WZ/FtA==\n\
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

// Created as follows:
// 1. Store serverCACertPEM in file testcert.pem
// 2. openssl x509 -in testcert.pem -outform DER -out - | xxd --include
const std::vector<unsigned char> caCertificateDerVector= {
  0x30, 0x82, 0x06, 0x1c, 0x30, 0x82, 0x04, 0x04, 0xa0, 0x03, 0x02, 0x01,
  0x02, 0x02, 0x14, 0x08, 0x7b, 0x2f, 0xfd, 0x75, 0xa1, 0xf2, 0x4b, 0xaa,
  0x1a, 0xd7, 0x47, 0xad, 0x03, 0x49, 0xa7, 0x82, 0xc7, 0xaf, 0x55, 0x30,
  0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
  0x05, 0x00, 0x30, 0x81, 0x80, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
  0x04, 0x06, 0x13, 0x02, 0x4e, 0x4c, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
  0x55, 0x04, 0x08, 0x0c, 0x0a, 0x47, 0x65, 0x6c, 0x64, 0x65, 0x72, 0x6c,
  0x61, 0x6e, 0x64, 0x31, 0x11, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x04, 0x07,
  0x0c, 0x08, 0x4e, 0x69, 0x6a, 0x6d, 0x65, 0x67, 0x65, 0x6e, 0x31, 0x1d,
  0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x14, 0x52, 0x61, 0x64,
  0x62, 0x6f, 0x75, 0x64, 0x20, 0x55, 0x6e, 0x69, 0x76, 0x65, 0x72, 0x73,
  0x69, 0x74, 0x65, 0x69, 0x74, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55,
  0x04, 0x0b, 0x0c, 0x0b, 0x50, 0x45, 0x50, 0x20, 0x52, 0x6f, 0x6f, 0x74,
  0x20, 0x43, 0x41, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03,
  0x0c, 0x0b, 0x50, 0x45, 0x50, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43,
  0x41, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x34, 0x31, 0x30, 0x33, 0x30, 0x31,
  0x35, 0x33, 0x36, 0x34, 0x39, 0x5a, 0x17, 0x0d, 0x32, 0x35, 0x31, 0x30,
  0x33, 0x30, 0x31, 0x35, 0x33, 0x36, 0x34, 0x39, 0x5a, 0x30, 0x81, 0xa6,
  0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x4e,
  0x4c, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x0a,
  0x47, 0x65, 0x6c, 0x64, 0x65, 0x72, 0x6c, 0x61, 0x6e, 0x64, 0x31, 0x11,
  0x30, 0x0f, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0c, 0x08, 0x4e, 0x69, 0x6a,
  0x6d, 0x65, 0x67, 0x65, 0x6e, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55,
  0x04, 0x0a, 0x0c, 0x14, 0x52, 0x61, 0x64, 0x62, 0x6f, 0x75, 0x64, 0x20,
  0x55, 0x6e, 0x69, 0x76, 0x65, 0x72, 0x73, 0x69, 0x74, 0x65, 0x69, 0x74,
  0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c, 0x1e, 0x50,
  0x45, 0x50, 0x20, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6d, 0x65, 0x64, 0x69,
  0x61, 0x74, 0x65, 0x20, 0x50, 0x45, 0x50, 0x20, 0x53, 0x65, 0x72, 0x76,
  0x65, 0x72, 0x20, 0x43, 0x41, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55,
  0x04, 0x03, 0x0c, 0x1e, 0x50, 0x45, 0x50, 0x20, 0x49, 0x6e, 0x74, 0x65,
  0x72, 0x6d, 0x65, 0x64, 0x69, 0x61, 0x74, 0x65, 0x20, 0x50, 0x45, 0x50,
  0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x43, 0x41, 0x30, 0x82,
  0x02, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
  0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00, 0x30, 0x82,
  0x02, 0x0a, 0x02, 0x82, 0x02, 0x01, 0x00, 0xa7, 0xc5, 0xf8, 0xbb, 0xf2,
  0xdd, 0x2c, 0x63, 0x49, 0xca, 0x99, 0xfd, 0x66, 0x6e, 0x6a, 0x0f, 0x72,
  0x9d, 0x2f, 0x67, 0xa1, 0x85, 0xab, 0x9e, 0x7e, 0x79, 0x5f, 0xd7, 0x5d,
  0xbe, 0x91, 0xbe, 0xa4, 0xac, 0x02, 0x9b, 0x28, 0xb3, 0xf6, 0x82, 0x45,
  0x1e, 0xd5, 0xbe, 0x80, 0x3a, 0x90, 0xf6, 0x82, 0xf5, 0xa3, 0xc8, 0x2c,
  0x84, 0x7b, 0x17, 0xdc, 0x75, 0x6e, 0x28, 0x06, 0xa6, 0x06, 0xf7, 0x44,
  0x9c, 0x71, 0xa9, 0xda, 0x52, 0x9f, 0x13, 0xe9, 0x05, 0xda, 0x03, 0x7f,
  0x35, 0x70, 0x45, 0x65, 0x51, 0x20, 0xa7, 0x9b, 0xeb, 0xe2, 0xb6, 0x28,
  0x8d, 0x9b, 0x26, 0x41, 0x83, 0xaf, 0xb3, 0x5c, 0x6c, 0xcb, 0xd7, 0x86,
  0xb1, 0x4b, 0xc3, 0x45, 0x28, 0x7e, 0x72, 0x57, 0x0d, 0xce, 0x99, 0x22,
  0x40, 0x0b, 0x13, 0x68, 0xb7, 0xdf, 0xb0, 0xf6, 0x5b, 0xf2, 0x66, 0x20,
  0xb6, 0x58, 0xb9, 0xd6, 0x63, 0xab, 0xbe, 0x61, 0x6a, 0x91, 0x06, 0x8c,
  0xd1, 0x9c, 0xed, 0x39, 0xce, 0xa0, 0x5e, 0x6c, 0xc3, 0xe5, 0x37, 0xf8,
  0xf6, 0x49, 0x44, 0x1a, 0x19, 0x9f, 0x51, 0x07, 0x74, 0x25, 0x1e, 0x3b,
  0x0d, 0x11, 0x5f, 0x77, 0x9a, 0x0f, 0x6b, 0xfa, 0x77, 0xdf, 0x10, 0x19,
  0x74, 0xd2, 0x01, 0x01, 0x57, 0x3e, 0x1d, 0x0c, 0x79, 0xc1, 0x16, 0x93,
  0xad, 0x7f, 0x10, 0xf6, 0xa5, 0x81, 0x3f, 0x12, 0xff, 0x42, 0x21, 0x81,
  0x78, 0x46, 0xf5, 0x51, 0xd3, 0x68, 0x5d, 0x1b, 0x95, 0x9b, 0xf2, 0x20,
  0x7d, 0x86, 0xa3, 0xae, 0x61, 0xc8, 0x54, 0xe4, 0x12, 0x0f, 0xad, 0x3f,
  0xde, 0x54, 0x47, 0x82, 0xbf, 0x97, 0x5b, 0xbf, 0xee, 0xa5, 0xc1, 0xdb,
  0xbd, 0x27, 0x5e, 0x94, 0x13, 0x10, 0x8e, 0x84, 0x31, 0xbd, 0xb5, 0x86,
  0xdd, 0xf7, 0x87, 0xbb, 0xa7, 0xec, 0x78, 0x4a, 0x17, 0x00, 0xb4, 0xb7,
  0xd4, 0x54, 0x72, 0x82, 0xe4, 0x6d, 0x1c, 0xa8, 0x58, 0x27, 0x4b, 0xb3,
  0xb6, 0xee, 0x9c, 0x1a, 0x6c, 0xb5, 0x12, 0x9d, 0x17, 0xbf, 0x9d, 0xc8,
  0x5d, 0x9e, 0x3e, 0x8c, 0x5f, 0x36, 0x53, 0xb5, 0xf5, 0xec, 0xb4, 0x8b,
  0xdb, 0x31, 0xf7, 0x97, 0x77, 0x56, 0x79, 0xdf, 0xd2, 0x16, 0xd8, 0xd8,
  0x70, 0xa5, 0x86, 0x96, 0x39, 0x4a, 0x28, 0x73, 0x2b, 0xcf, 0xcc, 0xc3,
  0x19, 0xb5, 0x67, 0xf6, 0xd9, 0x82, 0xc4, 0x58, 0x8f, 0xb8, 0x60, 0x1b,
  0x8d, 0xde, 0xa5, 0xd7, 0x45, 0x16, 0x8b, 0x31, 0x61, 0xf9, 0x78, 0xd5,
  0x06, 0x40, 0xdc, 0x3a, 0x11, 0x9e, 0xd7, 0x1d, 0x4a, 0x3f, 0x83, 0x6e,
  0xf7, 0x5d, 0x33, 0x45, 0x3f, 0x43, 0x15, 0xe6, 0x9b, 0x33, 0xb1, 0x13,
  0x1b, 0x67, 0x76, 0x7e, 0xb5, 0x0a, 0x08, 0xb1, 0xa4, 0x53, 0x82, 0x22,
  0x86, 0x4e, 0x69, 0x96, 0x89, 0x3b, 0x33, 0xfe, 0xbc, 0x94, 0x1a, 0x23,
  0x04, 0x78, 0xe3, 0x16, 0x43, 0x97, 0xe5, 0x8f, 0x84, 0xe2, 0x79, 0x3b,
  0x20, 0xdd, 0x6f, 0x7a, 0x0a, 0xa1, 0xce, 0x04, 0xb8, 0xcd, 0xa0, 0x08,
  0x84, 0xd1, 0xfb, 0x68, 0xa8, 0x7b, 0xc5, 0x85, 0xa0, 0x5a, 0x07, 0xd7,
  0xfb, 0xe3, 0x73, 0xc1, 0x52, 0xe5, 0x4f, 0xa6, 0x98, 0xb0, 0x29, 0x9e,
  0x06, 0xe1, 0x1c, 0x4e, 0x6c, 0xed, 0x09, 0xe0, 0xae, 0x32, 0xe3, 0x88,
  0xe2, 0x67, 0x34, 0xc5, 0xae, 0x73, 0x15, 0xb0, 0xd1, 0xd4, 0x57, 0xed,
  0x21, 0x89, 0x88, 0xae, 0x1e, 0x74, 0xae, 0xaf, 0xbf, 0xae, 0x79, 0xf4,
  0x55, 0xc1, 0x8e, 0x84, 0xc4, 0x5d, 0xb5, 0xda, 0xb8, 0x85, 0xf1, 0x4c,
  0x58, 0x5d, 0xa0, 0xe7, 0xbd, 0xe8, 0xda, 0x7a, 0x30, 0xdd, 0xa1, 0xcc,
  0x89, 0x2e, 0x47, 0x11, 0x79, 0x0e, 0x3c, 0xd6, 0x75, 0xc5, 0x78, 0x6c,
  0xc7, 0x59, 0x73, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x66, 0x30, 0x64,
  0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08,
  0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x00, 0x30, 0x1d, 0x06, 0x03,
  0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x91, 0x44, 0x71, 0x2b, 0x21,
  0x3f, 0x1c, 0x0e, 0x0a, 0xd9, 0x49, 0xe2, 0x3c, 0x1b, 0x4c, 0xf1, 0x43,
  0x22, 0xfa, 0x40, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18,
  0x30, 0x16, 0x80, 0x14, 0xf5, 0x42, 0xda, 0xcd, 0x7c, 0xec, 0x6f, 0xac,
  0xc4, 0xa6, 0x28, 0x8f, 0x9d, 0x5f, 0x38, 0x51, 0xdc, 0x6a, 0xa6, 0x3b,
  0x30, 0x0e, 0x06, 0x03, 0x55, 0x1d, 0x0f, 0x01, 0x01, 0xff, 0x04, 0x04,
  0x03, 0x02, 0x01, 0x86, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
  0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x82, 0x02, 0x01, 0x00,
  0x64, 0x3d, 0xf4, 0xfd, 0x1f, 0xf0, 0xef, 0xf4, 0xbc, 0x89, 0x63, 0x0b,
  0x97, 0x22, 0x98, 0x41, 0x51, 0x1d, 0xaf, 0x2f, 0xfa, 0xce, 0xc2, 0xd6,
  0x41, 0xf8, 0xc3, 0x33, 0x36, 0xca, 0x8c, 0x9b, 0xda, 0x50, 0xc7, 0x77,
  0xbb, 0x08, 0x33, 0xeb, 0xeb, 0xbf, 0xa7, 0x7a, 0xf7, 0x0e, 0x76, 0xf0,
  0xea, 0x92, 0x8c, 0x5c, 0xff, 0x2b, 0xf8, 0xbc, 0x4c, 0x21, 0xb7, 0x9c,
  0x8b, 0x95, 0xd0, 0xea, 0xce, 0x63, 0x47, 0xe5, 0xa5, 0x6e, 0xa6, 0x84,
  0xa0, 0x84, 0xab, 0xc7, 0xe9, 0xdb, 0x47, 0x7e, 0x55, 0x7b, 0xb6, 0x7f,
  0xfd, 0xab, 0xb2, 0xac, 0x31, 0x04, 0x60, 0xfd, 0x75, 0xd0, 0x37, 0x5f,
  0xbb, 0xf6, 0x1b, 0x92, 0x46, 0x40, 0x42, 0xe9, 0x98, 0xe6, 0x1f, 0xd0,
  0x05, 0xa6, 0xd8, 0x27, 0x0b, 0x8d, 0xa6, 0x54, 0xec, 0x9f, 0x25, 0x7f,
  0x97, 0xac, 0x5d, 0xdf, 0xf9, 0xc7, 0xdf, 0x91, 0xd0, 0x58, 0xa1, 0x8e,
  0x58, 0x69, 0xc4, 0x8b, 0xb6, 0x99, 0xad, 0xd6, 0xec, 0x98, 0x77, 0xee,
  0x07, 0x85, 0x14, 0x73, 0x9c, 0xdf, 0xe8, 0x94, 0xd1, 0x61, 0xf5, 0xca,
  0xd7, 0xb1, 0x83, 0x7b, 0x51, 0x98, 0x06, 0x22, 0x8f, 0x12, 0xa1, 0xd2,
  0x0b, 0xed, 0xe8, 0x36, 0x6a, 0x35, 0xf9, 0xbe, 0x8b, 0x80, 0x38, 0x78,
  0x30, 0xf5, 0xd1, 0x10, 0xa6, 0xdd, 0x8e, 0xd2, 0x3c, 0xfd, 0x12, 0x6f,
  0x36, 0x6c, 0x43, 0x19, 0x2f, 0x57, 0x8d, 0x95, 0x10, 0x03, 0x94, 0x52,
  0xef, 0x3d, 0x5a, 0xa5, 0x6d, 0xae, 0x6e, 0x7f, 0x93, 0xe9, 0x1c, 0xba,
  0x36, 0xa5, 0x9e, 0x42, 0x43, 0xbf, 0x05, 0x37, 0x86, 0x41, 0x72, 0x67,
  0xa5, 0x27, 0x22, 0xbf, 0x93, 0xae, 0x25, 0xcf, 0x02, 0x3a, 0x8b, 0x47,
  0x3c, 0x31, 0xde, 0xd2, 0xd2, 0x61, 0x47, 0xfb, 0xc8, 0x48, 0x91, 0xdc,
  0xe7, 0xf5, 0xc3, 0x16, 0x26, 0xcf, 0xc0, 0x16, 0x6a, 0xdf, 0x35, 0xe6,
  0x8c, 0x19, 0x7e, 0xb6, 0x79, 0x58, 0x07, 0x11, 0xdf, 0x11, 0x76, 0x88,
  0x1b, 0xd4, 0x36, 0x9d, 0xef, 0x89, 0x0d, 0x6f, 0x40, 0xf6, 0x85, 0xf3,
  0x98, 0xf6, 0xdb, 0xe4, 0x78, 0x15, 0xf9, 0x3a, 0x21, 0x1f, 0x6c, 0x3a,
  0xbb, 0x18, 0xef, 0xe9, 0x24, 0xa3, 0xd9, 0x90, 0x2d, 0xd2, 0xc1, 0xac,
  0x5c, 0x43, 0x0d, 0x1c, 0xaa, 0x51, 0xdc, 0xc2, 0xdc, 0x2e, 0x1a, 0x8a,
  0xb0, 0xed, 0xe6, 0xb4, 0x14, 0xff, 0x61, 0xf4, 0x28, 0x24, 0xcb, 0x41,
  0x7f, 0x6c, 0x72, 0xbb, 0x69, 0x6b, 0xa6, 0x73, 0xf5, 0x01, 0xc8, 0xdc,
  0xee, 0x42, 0x83, 0x40, 0x01, 0x6e, 0xee, 0x1f, 0xd9, 0x62, 0xca, 0x56,
  0x69, 0xca, 0xdd, 0x77, 0x6e, 0x67, 0x19, 0x36, 0xdf, 0xa5, 0x51, 0x2e,
  0x23, 0x05, 0x49, 0x80, 0x97, 0x1d, 0xb3, 0xd4, 0xaf, 0xa7, 0xd5, 0xea,
  0xcb, 0x36, 0x59, 0xc2, 0xc8, 0x0e, 0x5b, 0x63, 0x9a, 0xc2, 0xeb, 0x13,
  0x9a, 0x6f, 0x1d, 0x11, 0xab, 0xba, 0xf9, 0x7e, 0x8f, 0x2d, 0x81, 0xdd,
  0xbf, 0x0a, 0x65, 0x2e, 0xcc, 0xf6, 0x52, 0x71, 0xed, 0x0f, 0x29, 0xd6,
  0x28, 0x05, 0x5e, 0x82, 0x64, 0x93, 0x42, 0x40, 0x1e, 0xe1, 0x68, 0xa2,
  0x88, 0x1c, 0xe8, 0x38, 0x29, 0x84, 0x52, 0x88, 0x86, 0x01, 0x39, 0x6d,
  0x8d, 0x2d, 0x44, 0x74, 0xa2, 0xdd, 0x11, 0x9e, 0x1c, 0x48, 0x77, 0x1e,
  0x27, 0x01, 0xde, 0x9a, 0xe9, 0x92, 0x47, 0xfa, 0xc3, 0xc1, 0x82, 0x6e,
  0x39, 0xb2, 0x3e, 0x64, 0x9a, 0xe8, 0x31, 0x0f, 0x69, 0x20, 0x2a, 0x67,
  0xbc, 0xbe, 0x87, 0x10, 0x5d, 0xae, 0x09, 0xc5, 0x53, 0x05, 0xc2, 0x7e,
  0x59, 0x39, 0x99, 0x0b, 0x3f, 0x09, 0x13, 0xbe, 0xd4, 0xbe, 0x6c, 0x6f,
  0xdd, 0x59, 0x09, 0x77, 0x2c, 0x94, 0x1b, 0x57
};
const std::string caCertificateDer(reinterpret_cast<char const*>(caCertificateDerVector.data()), caCertificateDerVector.size());

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
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate caCertificate2(caCertificate);
  EXPECT_EQ(caCertificate.toPem(), caCertificate2.toPem());
}

TEST(X509CertificateTest, AssignmentOperator) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate caCertificate2;
  caCertificate2 = caCertificate;
  EXPECT_EQ(caCertificate.toPem(), caCertificate2.toPem());
}

TEST(X509CertificateTest, GetPublicKey) {
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::AsymmetricKey publicKey = caCertificate.getPublicKey();
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  EXPECT_TRUE(caPrivateKey.isPrivateKeyFor(publicKey));
}

TEST(X509CertificateTest, GetCommonName) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));
  
  EXPECT_EQ(cert.getCommonName(), testCN) << "CN in certificate does not match expected value";
}

TEST(X509CertificateTest, GetOrganizationalUnit) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

  EXPECT_EQ(cert.getOrganizationalUnit(), testOU) << "OU in certificate does not match expected value";
}

TEST(X509CertificateTest, GetIssuerCommonName) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

  EXPECT_EQ(cert.getIssuerCommonName(), "PEP Intermediate PEP Server CA") << "Issuer CN in certificate does not match expected value";
}

TEST(X509CertificateTest, DoesntHaveTLSServerEKU) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

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
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

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
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);

  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);
  pep::X509CertificateSigningRequest csr2(keyPair, testCN, testOU);

  // Hardcoded duration: 60 seconds
  std::chrono::seconds expectedDuration(60);
  // Hardcoded duration: 60 minutes in seconds
  std::chrono::seconds expectedDuration2(3600);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, expectedDuration);
  pep::X509Certificate cert2 = csr2.signCertificate(caCertificate, caPrivateKey, expectedDuration2);
  pep::X509Certificate expiredCert = pep::X509Certificate::FromPem(ExpiredLeafCertSignedWithserverCACertPEM);

  EXPECT_TRUE(cert.isCurrentTimeInValidityPeriod()) << "Certificate should be within the validity period";
  EXPECT_TRUE(cert2.isCurrentTimeInValidityPeriod()) << "Certificate should be within the validity period";

  EXPECT_FALSE(expiredCert.isCurrentTimeInValidityPeriod()) << "Certificate should not be within the validity period";
}

TEST(X509CertificateTest, ToPEM) {
  pep::X509Certificate cert = pep::X509Certificate::FromPem(serverCACertPEM);
  EXPECT_EQ(cert.toPem(), serverCACertPEM);
}

TEST(X509CertificateTest, ToDER) {
  pep::X509Certificate cert = pep::X509Certificate::FromPem(serverCACertPEM);
  EXPECT_EQ(cert.toDer(), caCertificateDer);
}

TEST(X509CertificateSigningRequestTest, GenerationAndSigning) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  EXPECT_EQ(csr.getCommonName(), testCN) << "CN in CSR does not match input";
  EXPECT_EQ(csr.getOrganizationalUnit(), testOU) << "OU in CSR does not match input";

  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);

  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

  EXPECT_EQ(cert.getCommonName(), testCN) << "CN in certificate does not match input";
  EXPECT_EQ(cert.getOrganizationalUnit(), testOU) << "OU in certificate does not match input";
}

TEST(X509CertificateSigningRequestTest, CertificateDuration) {
  std::string testCN = "TestCN";
  std::string testOU = "TestOU";
  pep::AsymmetricKeyPair keyPair = pep::AsymmetricKeyPair::GenerateKeyPair();
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);

  pep::X509CertificateSigningRequest csr(keyPair, testCN, testOU);

  // Duration above 2 years
  std::chrono::seconds invalidMaximumDuration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours{2 * 365 * 24 + 1});
  std::chrono::seconds invalidMinimumDuration = std::chrono::seconds(-1);

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
  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));
  
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
  pep::X509Certificate intermediateCACert = pep::X509Certificate::FromPem(serverCACertPEM);
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

  pep::AsymmetricKey caPrivateKey(serverCAPrivateKeyPEM);
  pep::X509Certificate caCertificate = pep::X509Certificate::FromPem(serverCACertPEM);

  // Sign the certificate
  pep::X509Certificate cert = csr.signCertificate(caCertificate, caPrivateKey, std::chrono::seconds(60));

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
  EXPECT_NO_THROW(pep::X509Certificates certificates("extra text\n" + authserverCertPEM + serverCACertPEM));
  EXPECT_NO_THROW(pep::X509Certificates certificates(authserverCertPEM + "extra text\n" + serverCACertPEM));

  // But bad formatting after a -----BEGIN CERTIFICATE----- block should produce an error
  EXPECT_THROW(pep::X509Certificates certificates(authserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting\n-----END CERTIFICATE-----" + serverCACertPEM), std::runtime_error);
  // Also without a -----END CERTIFICATE----- block
  EXPECT_THROW(pep::X509Certificates certificates(authserverCertPEM + "-----BEGIN CERTIFICATE-----\nbad formatting" + serverCACertPEM), std::runtime_error);
  // But bad formatting with only an -----END CERTIFICATE----- doesnt throw an error
  EXPECT_NO_THROW(pep::X509Certificates certificates(authserverCertPEM + "bad formatting\n-----END CERTIFICATE-----\n" + serverCACertPEM));

  // Openssl error should be cleared after parsing errors
  EXPECT_TRUE(ERR_get_error() == 0) << "Openssl errors are not cleared after parsing errors";

  // If the previous test fails, still make sure the rest of the tests have the correct error queue state, if it didn't fail the error queue is empty anyway
  ERR_clear_error();
}

TEST(X509CertificatesTest, ToPem) {
  pep::X509Certificates certificates(authserverCertPEM + serverCACertPEM);
  std::string expectedPem = authserverCertPEM + serverCACertPEM;
  EXPECT_EQ(certificates.toPem(), expectedPem) << "PEM conversion of X509Certificates failed";
}

TEST(X509CertificateChainTest, VerifyCertificateChain) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEM);

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(authserverCertPEM + serverCACertPEM);

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
  pep::X509CertificateChain certChain(ExpiredLeafCertSignedWithserverCACertPEM + serverCACertPEM);

  // Verify the certificate chain against the root CAs
  EXPECT_FALSE(certChain.verify(rootCA)) << "Certificate chain verification succeeded with expired leaf certificate";
}

TEST(X509CertificateChainTest, VerifyCertificateChainOrdering) {
  // Load the root CA certificate
  pep::X509RootCertificates rootCA(rootCACertPEM);

  // Load the intermediate and server certificates
  pep::X509CertificateChain certChain(serverCACertPEM + authserverCertPEM);

  // Verify the certificate chain against the root CAs
  EXPECT_TRUE(certChain.verify(rootCA)) << "Certificate chain verification failed for reverse ordering";
}

TEST(X509CertificateChainTest, CertifiesPrivateKey) {
  pep::X509CertificateChain certChain(serverCACertPEM + rootCACertPEM);
  pep::AsymmetricKey privateKey(serverCAPrivateKeyPEM);
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
