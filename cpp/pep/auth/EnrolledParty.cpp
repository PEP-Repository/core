#include <pep/auth/ServerTraits.hpp>

namespace pep {

namespace {

const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

}

std::optional<EnrolledParty> GetEnrolledParty(const X509Certificate& certificate) {
  if (IsUserSigningCertificate(certificate)) {
    return EnrolledParty::User;
  }

  if (auto server = ServerTraits::ForCertificate(certificate)) {
    return server->enrollsAs();
  }

  return std::nullopt;
}

std::optional<EnrolledParty> GetEnrolledParty(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return std::nullopt;
  }
  return GetEnrolledParty(*chain.begin());
}

bool IsUserSigningCertificate(const X509Certificate& certificate) {
  return certificate.getIssuerCommonName() == intermediateClientCaCommonName;
}

bool HasDataAccess(EnrolledParty party) {
  if (party == EnrolledParty::User) {
    return true;
  }
  if (auto server = ServerTraits::Find(party)) {
    return server->hasDataAccess();
  }
  return false;
}

}
