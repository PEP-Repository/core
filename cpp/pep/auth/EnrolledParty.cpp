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
  if (certificate.isPEPUserCertificate()) {
    return EnrolledParty::User;
  }
  if (certificate.hasTLSServerEKU()) {
    return EnrolledParty::Unknown;
  }
  if (!certificate.isPEPServerCertificate()) {
    return EnrolledParty::Unknown;
  }

  auto cn = certificate.getCommonName();
  assert(cn.has_value());
  auto mapping = certificateSubjectMappings.left.find(*cn);
  if (mapping == certificateSubjectMappings.left.end()) {
    return EnrolledParty::Unknown;
  }

  return mapping->second;
}

EnrolledParty GetEnrolledParty(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return EnrolledParty::Unknown;
  }
  return GetEnrolledParty(*chain.begin());
}

}
