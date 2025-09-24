#include <pep/auth/FacilityType.hpp>

#include <boost/bimap/bimap.hpp>

#include <array>

namespace pep {

namespace {

const auto certificateSubjectMappings = [] {
  using map_type = boost::bimaps::bimap<std::string, FacilityType>;
  std::array<map_type::value_type, 4> pairs{{
    {"StorageFacility", FacilityType::StorageFacility},
    {"AccessManager", FacilityType::AccessManager},
    {"RegistrationServer", FacilityType::RegistrationServer},
    {"Transcryptor", FacilityType::Transcryptor},
  }};
  return map_type{pairs.begin(), pairs.end()};
}();

const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

} // namespace

FacilityType CertificateSubjectToFacilityType(const std::string& commonName, const std::string& organizationalUnit) {
  if (commonName != organizationalUnit) {
    // Server facilities are enrolled with equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
    return FacilityType::Unknown;
  }

  if (auto it = certificateSubjectMappings.left.find(organizationalUnit);
    it != certificateSubjectMappings.left.end()) {
    return it->second;
  }
  return FacilityType::Unknown;
}

std::optional<std::string_view> FacilityTypeToCertificateSubject(FacilityType facilityType) {
  if (auto it = certificateSubjectMappings.right.find(facilityType);
      it != certificateSubjectMappings.right.end()) {
    return it->second;
  }
  return {};
}

FacilityType GetFacilityType(const X509Certificate& certificate) {
  auto result = CertificateSubjectToFacilityType(certificate.getCommonName().value_or(""), certificate.getOrganizationalUnit().value_or(""));
  switch (result) {
  case FacilityType::Unknown:
  case FacilityType::User:
    if (certificate.getIssuerCommonName() == intermediateClientCaCommonName) {
      return FacilityType::User;
    } else {
      return FacilityType::Unknown;
    }
  case FacilityType::StorageFacility:
  case FacilityType::AccessManager:
  case FacilityType::Transcryptor:
  case FacilityType::RegistrationServer:
    if (certificate.isPEPServerCertificate()) {
      return result;
    } else {
      return FacilityType::Unknown;
    }
  }
  return FacilityType::Unknown;
}

FacilityType GetFacilityType(const X509CertificateChain& chain) {
  if (chain.empty()) {
    return FacilityType::Unknown;
  }
  return GetFacilityType(*chain.begin());
}

}
