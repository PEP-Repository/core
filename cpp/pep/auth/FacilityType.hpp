#pragma once

#include <pep/crypto/X509Certificate.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pep {

enum class FacilityType : uint32_t {
  Unknown = 0,
  User = 1,
  StorageFacility = 2,
  AccessManager = 3,
  Transcryptor = 4,
  RegistrationServer = 5,
};

std::optional<std::string_view> FacilityTypeToCertificateSubject(FacilityType facilityType);
FacilityType CertificateSubjectToFacilityType(const std::string& commonName, const std::string& organizationalUnit);
FacilityType GetFacilityType(const X509Certificate& certificate); // Inferred from the certificate's OU, CN, and issuer CN.
FacilityType GetFacilityType(const X509CertificateChain& chain);

}
