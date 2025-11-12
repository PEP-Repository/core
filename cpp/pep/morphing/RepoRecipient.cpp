#include <pep/morphing/RepoRecipient.hpp>

#include <pep/auth/EnrolledParty.hpp>

#include <cassert>
#include <stdexcept>
#include <string_view>

using namespace pep;

namespace {

std::string_view GetValidServerCertificateSubject(EnrolledParty server) {
  const auto serverName = GetEnrolledServerCertificateSubject(server);
  if (!serverName) {
    throw std::invalid_argument("EnrolledParty is not a server");
  }
  return *serverName;
}

EnrolledParty GetValidEnrolledParty(const X509Certificate& cert) {
  auto party = GetEnrolledParty(cert);
  if (!party.has_value()) {
    throw std::invalid_argument("EnrolledParty is unknown");
  }
  return *party;
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
std::string RekeyPayload(const X509Certificate& cert, EnrolledParty party) {
  assert(GetEnrolledParty(cert) == party);
  if (party == EnrolledParty::User) {
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
      static_cast<RecipientBase::Type>(GetValidEnrolledParty(cert)),
      ReshufflePayload(cert),
  };
}

ReshuffleRecipient pep::PseudonymRecipientForUserGroup(std::string userGroup) {
  return {
      static_cast<RecipientBase::Type>(EnrolledParty::User),
      std::move(userGroup),
  };
}

ReshuffleRecipient pep::PseudonymRecipientForServer(const EnrolledParty& server) {
  return {
      static_cast<RecipientBase::Type>(server),
      std::string(GetValidServerCertificateSubject(server)),
  };
}

RekeyRecipient pep::RekeyRecipientForCertificate(const X509Certificate& cert) {
  EnrolledParty party = GetValidEnrolledParty(cert);
  return {
      static_cast<RecipientBase::Type>(party),
      RekeyPayload(cert, party),
  };
}

RekeyRecipient pep::RekeyRecipientForServer(const EnrolledParty& server) {
  return {
      static_cast<RecipientBase::Type>(server),
      std::string(GetValidServerCertificateSubject(server)),
  };
}

SkRecipient pep::RecipientForCertificate(const X509Certificate& cert) {
  EnrolledParty party = GetValidEnrolledParty(cert);
  return {
      static_cast<RecipientBase::Type>(party),
      {
          .reshuffle = ReshufflePayload(cert),
          .rekey = RekeyPayload(cert, party),
      },
  };
}

SkRecipient pep::RecipientForServer(const EnrolledParty& server) {
  auto serverName = GetValidServerCertificateSubject(server);
  return {
      static_cast<RecipientBase::Type>(server),
      {
          .reshuffle = std::string(serverName),
          .rekey = std::string(serverName),
      },
  };
}
