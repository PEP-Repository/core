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

std::optional<EnrolledParty> GetEnrolledServer(const std::string& commonName, const std::string& organizationalUnit) {
  if (commonName != organizationalUnit) {
    // Server facilities are enrolled with equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
    return std::nullopt;
  }

  if (auto it = certificateSubjectMappings.left.find(organizationalUnit);
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
  if (certificate.isPEPUserCertificate()) {
    return EnrolledParty::User;
  }
  if (certificate.hasTLSServerEKU()) {
    return std::nullopt;
  }
  if (!certificate.isPEPServerCertificate()) {
    return std::nullopt;
  }

  auto cn = certificate.getCommonName();
  assert(cn.has_value());
  auto mapping = certificateSubjectMappings.left.find(*cn);
  if (mapping == certificateSubjectMappings.left.end()) {
    return std::nullopt;
  }

  return mapping->second;
}

std::optional<EnrolledParty> GetEnrolledParty(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return std::nullopt;
  }
  return GetEnrolledParty(*chain.begin());
}

bool EnrolledPartyHasDataAccess(EnrolledParty party) {
  switch (party) {
  case EnrolledParty::User:
  case EnrolledParty::RegistrationServer:
    return true;
  default:
    return false;
  }
}

}
