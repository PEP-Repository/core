#pragma once

#include <pep/crypto/X509Certificate.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pep {

enum class EnrolledParty : uint32_t {
  User = 1,
  StorageFacility = 2,
  AccessManager = 3,
  Transcryptor = 4,
  RegistrationServer = 5,
  AuthServer = 6,
};

std::optional<std::string_view> GetEnrolledServerCertificateSubject(EnrolledParty party);
std::optional<EnrolledParty> GetEnrolledServer(const std::string& ou);

std::optional<EnrolledParty> GetEnrolledParty(const X509Certificate& certificate); // Inferred from the certificate's OU, CN, and issuer CN.
std::optional<EnrolledParty> GetEnrolledParty(const X509CertificateChain& chain);

bool IsServerTlsCertificate(const X509Certificate& certificate);
bool IsServerEnrollmentCertificate(const X509Certificate& certificate);
bool IsUserEnrollmentCertificate(const X509Certificate& certificate);

bool HasDataAccess(EnrolledParty party);

}
