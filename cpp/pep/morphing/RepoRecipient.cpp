#include <pep/morphing/RepoRecipient.hpp>

#include <pep/auth/FacilityType.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <cassert>
#include <stdexcept>
#include <string_view>

using namespace pep;

namespace {

std::string_view GetValidServerName(FacilityType serverFacilityType) {
  const auto serverName = FacilityTypeToCertificateSubject(serverFacilityType);
  if (!serverName) {
    throw std::invalid_argument("FacilityType is not a server");
  }
  return *serverName;
}

FacilityType GetValidFacilityType(const X509Certificate& cert) {
  FacilityType type = GetFacilityType(cert);
  if (type == FacilityType::Unknown) {
    throw std::invalid_argument("FacilityType is unknown");
  }
  return type;
}

/// Reshuffle: take user group name for users, as pseudonymization is per user group
std::string ReshufflePayload(const X509Certificate& cert) {
  if (cert.getOrganizationalUnit().has_value()) {
    return cert.getOrganizationalUnit().value();
  } else {
    throw std::runtime_error("Missing Organizational Unit in the certificate.");
  }
}

/// Rekey: take certificate serialization for users, as rekeying is per user
std::string RekeyPayload(const X509Certificate& cert, FacilityType type) {
  assert(GetFacilityType(cert) == type);
  if (type == FacilityType::User) {
    return cert.toDer();
  } else {
    if (cert.getOrganizationalUnit().has_value()) {
      return cert.getOrganizationalUnit().value();
    } else {
      throw std::runtime_error("Missing Organizational Unit in the certificate.");
    }
  }
}

} // namespace

ReshuffleRecipient pep::PseudonymRecipientForCertificate(const X509Certificate& cert) {
  return {
      static_cast<RecipientBase::Type>(GetValidFacilityType(cert)),
      ReshufflePayload(cert),
  };
}

ReshuffleRecipient pep::PseudonymRecipientForUserGroup(std::string userGroup) {
  return {
      static_cast<RecipientBase::Type>(FacilityType::User),
      std::move(userGroup),
  };
}

ReshuffleRecipient pep::PseudonymRecipientForServer(const FacilityType& server) {
  return {
      static_cast<RecipientBase::Type>(server),
      std::string(GetValidServerName(server)),
  };
}

RekeyRecipient pep::RekeyRecipientForCertificate(const X509Certificate& cert) {
  FacilityType type = GetValidFacilityType(cert);
  return {
      static_cast<RecipientBase::Type>(type),
      RekeyPayload(cert, type),
  };
}

RekeyRecipient pep::RekeyRecipientForServer(const FacilityType& server) {
  return {
      static_cast<RecipientBase::Type>(server),
      std::string(GetValidServerName(server)),
  };
}

SkRecipient pep::RecipientForCertificate(const X509Certificate& cert) {
  FacilityType type = GetValidFacilityType(cert);
  return {
      static_cast<RecipientBase::Type>(type),
      {
          .reshuffle = ReshufflePayload(cert),
          .rekey = RekeyPayload(cert, type),
      },
  };
}

SkRecipient pep::RecipientForServer(const FacilityType& server) {
  auto serverName = GetValidServerName(server);
  return {
      static_cast<RecipientBase::Type>(server),
      {
          .reshuffle = std::string(serverName),
          .rekey = std::string(serverName),
      },
  };
}
