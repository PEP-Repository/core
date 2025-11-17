#include <pep/auth/Certificate.hpp>
#include <cassert>

namespace pep {

namespace {

const std::string intermediateServerTlsCaCommonName = "PEP Intermediate TLS CA";
const std::string intermediateServerCaCommonName = "PEP Intermediate PEP Server CA";
const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

bool CertificateMatchesCA(const X509Certificate& certificate, const std::string& cn, bool tls) {
  if (certificate.getIssuerCommonName() != cn) {
    return false;
  }

  // TLS CA should only issue TLS certificates; non-TLS CA only non-TLS certificates
  auto result = tls == certificate.hasTLSServerEKU();
  assert(result);
  return result;
}

std::optional<std::string> GetServerSubjectIfCaMatches(const X509Certificate& certificate, const std::string& cn, bool tls) {
  if (!CertificateMatchesCA(certificate, cn, tls)) {
    return std::nullopt;
  }

  auto result = certificate.getOrganizationalUnit();

  // CA should have included an OU in the certificate
  assert(result.has_value());
  if (!result.has_value()) {
    return std::nullopt;
  }

  // Server certificates have equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
  assert(result == certificate.getCommonName());
  if (result != certificate.getCommonName()) {
    return std::nullopt;
  }

  return result;
}

}

bool IsServerTlsCertificate(const X509Certificate& certificate) {
  return GetServerSubjectIfCaMatches(certificate, intermediateServerTlsCaCommonName, true).has_value();
}

bool IsServerSigningCertificate(const X509Certificate& certificate) {
  return GetServerSubjectIfCaMatches(certificate, intermediateServerCaCommonName, false).has_value();
}

bool IsUserSigningCertificate(const X509Certificate& certificate) {
  return CertificateMatchesCA(certificate, intermediateClientCaCommonName, false);
}

std::optional<std::string> GetSubjectIfServerSigningCertificate(const X509Certificate& certificate) {
  return GetServerSubjectIfCaMatches(certificate, intermediateServerCaCommonName, false);
}

std::optional<std::string> GetSubjectIfServerSigningCertificate(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return std::nullopt;
  }
  return GetSubjectIfServerSigningCertificate(chain.front());
}

}
