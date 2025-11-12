#include <pep/auth/ServerTraits.hpp>

namespace pep {

namespace {

const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

}

std::optional<EnrolledParty> GetEnrolledParty(const X509Certificate& certificate) {
  if (IsUserSigningCertificate(certificate)) {
    return EnrolledParty::User;
  }

  auto server = ServerTraits::ForCertificate(certificate);
  if (!server.has_value()) {
    return std::nullopt;
  }

  return server->enrollsAs();
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
