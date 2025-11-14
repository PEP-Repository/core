#include <pep/auth/ServerTraits.hpp>
#include <cassert>

namespace pep {

namespace {

const std::string intermediateServerCaCommonName = "PEP Intermediate PEP Server CA";
const std::string intermediateServerTlsCaCommonName = "PEP Intermediate TLS CA";

std::optional<std::string> GetServerSigningCertificateSubject(const X509Certificate& certificate) {
  if (certificate.hasTLSServerEKU()) {
    return std::nullopt; // Not the correct type of certificate
  }
  if (certificate.getIssuerCommonName() != intermediateServerCaCommonName) {
    return std::nullopt; // Not issued by the correct intermediate CA
  }

  auto result = certificate.getOrganizationalUnit();
  if (!result.has_value()) {
    return std::nullopt; // Empty OU (user group)
  }
  if (result != certificate.getCommonName()) {
    return std::nullopt; // Server certificates have equal CN and OU, e.g. "OU=AccessManager, CN=AccessManager"
  }

  return result;
}

}

ServerTraits::ServerTraits(std::string description) noexcept
  : mDescription(std::move(description)) {
}

ServerTraits::ServerTraits(std::string description, EnrolledParty enrollsAs) noexcept
  : mDescription(std::move(description)), mEnrollsAs(enrollsAs) {
  assert(mEnrollsAs != EnrolledParty::User);
}

ServerTraits::ServerTraits(std::string description, std::string customId) noexcept
  : mDescription(std::move(description)), mCustomId(std::move(customId)) {
}

std::string ServerTraits::defaultId() const {
  auto result = mDescription;
  result.erase(remove_if(result.begin(), result.end(), isspace), result.end());
  return result;
}

std::string ServerTraits::id() const {
  if (mCustomId.has_value()) {
    return *mCustomId;
  }
  return this->defaultId();
}

bool ServerTraits::matchesCertificateSubject(const std::string& subject) const {
  return this->userGroups().contains(subject);
}

bool ServerTraits::matchesCertificate(const X509Certificate& certificate) const {
  if (auto subject = GetServerSigningCertificateSubject(certificate)) {
    return this->matchesCertificateSubject(*subject);
  }
  return false;
}

std::string ServerTraits::tlsCertificateSubject() const {
  return this->id();
}

std::unordered_set<std::string> ServerTraits::userGroups() const {
  std::unordered_set<std::string> result;
  if (mCustomId.has_value()) {
    result.emplace(*mCustomId); // Legacy: PEP (enrollment) certificates have been issued for "Authserver" not "AuthServer"
  }

  if (mEnrollsAs.has_value() || mCustomId.has_value()) {
    result.emplace(this->defaultId());
  }

  return result;

}

bool ServerTraits::matchesCertificateChain(const X509CertificateChain& chain) const {
  if (chain.empty()) {
    return false;
  }
  return this->matchesCertificate(chain.front());
}

std::optional<std::string> ServerTraits::enrollmentSubject() const {
  if (!mEnrollsAs.has_value()) {
    return std::nullopt;
  }
  return this->defaultId();
}

bool ServerTraits::hasDataAccess() const {
  return mEnrollsAs == EnrolledParty::RegistrationServer;
}

std::string ServerTraits::configNode() const {
  return this->id();
}

std::string ServerTraits::commandLineId() const {
  auto result = this->id();
  std::for_each(result.begin(), result.end(), [](char& c) { c = static_cast<char>(std::tolower(c)); });
  return result;
}

ServerTraits ServerTraits::AccessManager()      { return ServerTraits{ "Access Manager",      EnrolledParty::AccessManager       }; }
ServerTraits ServerTraits::AuthServer()         { return ServerTraits{ "Auth Server",         "Authserver"                       }; }
ServerTraits ServerTraits::KeyServer()          { return ServerTraits{ "Key Server"                                              }; }
ServerTraits ServerTraits::RegistrationServer() { return ServerTraits{ "Registration Server", EnrolledParty::RegistrationServer, }; }
ServerTraits ServerTraits::StorageFacility()    { return ServerTraits{ "Storage Facility",    EnrolledParty::StorageFacility     }; }
ServerTraits ServerTraits::Transcryptor()       { return ServerTraits{ "Transcryptor",        EnrolledParty::Transcryptor,       }; }

std::vector<ServerTraits> ServerTraits::All() {
  return {
    AccessManager(),
    AuthServer(),
    KeyServer(),
    RegistrationServer(),
    StorageFacility(),
    Transcryptor(),
  };
}

std::optional<ServerTraits> ServerTraits::Find(const std::function<bool(const ServerTraits&)>& predicate) {
  auto all = All();
  auto end = all.end();
  auto position = std::find_if(all.begin(), end, predicate);
  if (position != end) {
    return *position;
  }
  return std::nullopt;
}

std::optional<ServerTraits> ServerTraits::Find(EnrolledParty enrollsAs) {
  return Find([enrollsAs](const ServerTraits& candidate) {return candidate.enrollsAs() == enrollsAs; });
}

std::optional<ServerTraits> ServerTraits::ForCertificateSubject(const std::string& subject) {
  return Find([subject](const ServerTraits& candidate) {return candidate.matchesCertificateSubject(subject); });
}

std::optional<ServerTraits> ServerTraits::ForCertificate(const X509Certificate& certificate) {
  if (auto subject = GetServerSigningCertificateSubject(certificate)) {
    return ForCertificateSubject(*subject);
  }
  return std::nullopt;
}

}
