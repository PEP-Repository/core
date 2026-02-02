#include <pep/auth/Certificate.hpp>
#include <pep/auth/ServerTraits.hpp>

namespace pep {

std::optional<EnrolledParty> GetEnrolledParty(const X509Certificate& certificate) {
  if (IsUserSigningCertificate(certificate)) {
    return EnrolledParty::User;
  }

  if (auto subject = GetSubjectIfServerSigningCertificate(certificate)) {
    if (auto traits = ServerTraits::Find([subject](const ServerTraits& candidate) {return candidate.signingIdentityMatches(*subject); })) {
      return traits->enrollsAsParty(false);
    }
  }

  return std::nullopt;
}

std::optional<EnrolledParty> GetEnrolledParty(const X509CertificateChain& chain) {
  return GetEnrolledParty(chain.leaf());
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
