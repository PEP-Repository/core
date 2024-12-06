#include <pep/auth/FacilityType.hpp>

#include <unordered_map>

namespace pep {

namespace {

const std::unordered_map<std::string, FacilityType> certificateSubjectMappings{
  {"StorageFacility",    FacilityType::StorageFacility},
  {"AccessManager",      FacilityType::AccessManager},
  {"RegistrationServer", FacilityType::RegistrationServer},
  {"Transcryptor",       FacilityType::Transcryptor},
};

const std::unordered_map<FacilityType, std::string> certificateSubjectReverseMappings = []() {
  std::unordered_map<FacilityType, std::string> result;
  result.reserve(certificateSubjectMappings.size());
  for (const auto& [subject, type]: certificateSubjectMappings) {
    result.emplace(type, subject);
  }
  return result;
}();

const std::string intermediateClientCaCommonName = "PEP Intermediate PEP Client CA";

} // namespace

FacilityType CertificateSubjectToFacilityType(const std::string& commonName, const std::string& organizationalUnit) {
  if (commonName != organizationalUnit) {
    // Server facilities are enrolled with equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
    return FacilityType::Unknown;
  }

  if (auto it = certificateSubjectMappings.find(organizationalUnit);
    it != certificateSubjectMappings.end()) {
    return it->second;
  }
  return FacilityType::Unknown;
}

std::optional<std::string_view> FacilityTypeToCertificateSubject(FacilityType facilityType) {
  if (auto it = certificateSubjectReverseMappings.find(facilityType);
      it != certificateSubjectReverseMappings.end()) {
    return it->second;
  }
  return {};
}

FacilityType GetFacilityType(const X509Certificate& certificate) {
  auto result = CertificateSubjectToFacilityType(certificate.getCommonName(), certificate.getOrganizationalUnit());
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
    if (certificate.isServerCertificate()) {
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
