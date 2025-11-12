#include <pep/auth/EnrolledParty.hpp>

#include <boost/bimap/bimap.hpp>

#include <array>

namespace pep {

namespace {

const auto certificateSubjectMappings = [] {
  using map_type = boost::bimaps::bimap<std::string, EnrolledParty>;
  std::array<map_type::value_type, 4> pairs{{
    {"StorageFacility", EnrolledParty::StorageFacility},
    {"AccessManager", EnrolledParty::AccessManager},
    {"RegistrationServer", EnrolledParty::RegistrationServer},
    {"Transcryptor", EnrolledParty::Transcryptor},
  }};
  return map_type{pairs.begin(), pairs.end()};
}();

const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

} // namespace

EnrolledParty CertificateSubjectToEnrolledParty(const std::string& commonName, const std::string& organizationalUnit) {
  if (commonName != organizationalUnit) {
    // Server facilities are enrolled with equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
    return EnrolledParty::Unknown;
  }

  if (auto it = certificateSubjectMappings.left.find(organizationalUnit);
    it != certificateSubjectMappings.left.end()) {
    return it->second;
  }
  return EnrolledParty::Unknown;
}

std::optional<std::string_view> EnrolledPartyToCertificateSubject(EnrolledParty party) {
  if (auto it = certificateSubjectMappings.right.find(party);
      it != certificateSubjectMappings.right.end()) {
    return it->second;
  }
  return {};
}

EnrolledParty GetEnrolledParty(const X509Certificate& certificate) {
  auto result = CertificateSubjectToEnrolledParty(certificate.getCommonName().value_or(""), certificate.getOrganizationalUnit().value_or(""));
  switch (result) {
  case EnrolledParty::Unknown:
  case EnrolledParty::User:
    if (certificate.getIssuerCommonName() == intermediateClientCaCommonName) {
      return EnrolledParty::User;
    } else {
      return EnrolledParty::Unknown;
    }
  case EnrolledParty::StorageFacility:
  case EnrolledParty::AccessManager:
  case EnrolledParty::Transcryptor:
  case EnrolledParty::RegistrationServer:
    if (certificate.isPEPServerCertificate()) {
      return result;
    } else {
      return EnrolledParty::Unknown;
    }
  }
  return EnrolledParty::Unknown;
}

EnrolledParty GetEnrolledParty(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return EnrolledParty::Unknown;
  }
  return GetEnrolledParty(*chain.begin());
}

}
