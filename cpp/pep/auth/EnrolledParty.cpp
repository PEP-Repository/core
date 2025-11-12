#include <pep/auth/EnrolledParty.hpp>
#include <pep/auth/UserGroup.hpp>

#include <boost/bimap/bimap.hpp>

#include <array>

namespace pep {

namespace {

const std::string intermediateServerCaCommonName = "PEP Intermediate PEP Server CA";
const std::string intermediateServerTlsCaCommonName = "PEP Intermediate TLS CA";
const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

const auto certificateSubjectMappings = [] {
  using map_type = boost::bimaps::bimap<std::string, EnrolledParty>;
  std::array<map_type::value_type, 4> pairs{ {
    {"StorageFacility", EnrolledParty::StorageFacility},
    {"AccessManager", EnrolledParty::AccessManager},
    {"RegistrationServer", EnrolledParty::RegistrationServer},
    {"Transcryptor", EnrolledParty::Transcryptor},
  }};
  return map_type{pairs.begin(), pairs.end()};
}();

std::optional<std::string> GetServerCertificateSubject(const X509Certificate& certificate) {
  if (certificate.hasTLSServerEKU()) {
    return std::nullopt; // Not the correct type of certificate
  }
  auto issuer = intermediateServerCaCommonName;
  if (certificate.getIssuerCommonName() != issuer) {
    return std::nullopt; // Not issued by the correct intermediate CA
  }

  auto result = certificate.getOrganizationalUnit();
  if (!result.has_value()) {
    return std::nullopt; // Empty OU (user group)
  }
  if (result != certificate.getCommonName()) {
    return std::nullopt; // Server facilities are enrolled with equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
  }

  return result;
}

} // namespace

std::optional<EnrolledParty> GetEnrolledServer(const std::string& ou) {
  if (auto it = certificateSubjectMappings.left.find(ou);
    it != certificateSubjectMappings.left.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<std::string_view> GetEnrolledServerCertificateSubject(EnrolledParty party) {
  if (auto it = certificateSubjectMappings.right.find(party);
      it != certificateSubjectMappings.right.end()) {
    return it->second;
  }
  return {};
}

std::optional<EnrolledParty> GetEnrolledParty(const X509Certificate& certificate) {
  if (IsUserSigningCertificate(certificate)) {
    return EnrolledParty::User;
  }

  auto subject = GetServerCertificateSubject(certificate);
  if (!subject.has_value()) {
    return std::nullopt;
  }

  return GetEnrolledServer(*subject);
}

std::optional<EnrolledParty> GetEnrolledParty(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return std::nullopt;
  }
  return GetEnrolledParty(*chain.begin());
}

bool IsServerSigningCertificate(const X509Certificate& certificate) {
  if (auto subject = GetServerCertificateSubject(certificate)) {
    if (certificateSubjectMappings.left.count(*subject) != 0) {
      return true;
    }
    return UserGroup::Authserver.contains(*subject);
  }
  return false;
}

bool IsUserSigningCertificate(const X509Certificate& certificate) {
  return certificate.getIssuerCommonName() == intermediateClientCaCommonName;
}

bool HasDataAccess(EnrolledParty party) {
  switch (party) {
  case EnrolledParty::User:
  case EnrolledParty::RegistrationServer:
    return true;
  default:
    return false;
  }
}

}
