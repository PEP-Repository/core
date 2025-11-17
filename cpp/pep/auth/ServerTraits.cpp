#include <pep/auth/Certificate.hpp>
#include <pep/auth/ServerTraits.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <cassert>

namespace pep {

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

bool ServerTraits::signingIdentityMatches(const std::string& certificateSubject) const {
  return this->userGroups().contains(certificateSubject);
}

bool ServerTraits::signingIdentityMatches(const X509Certificate& certificate) const {
  if (auto subject = GetSubjectIfServerSigningCertificate(certificate)) {
    return this->signingIdentityMatches(*subject);
  }
  return false;
}

std::string ServerTraits::tlsCertificateSubject() const {
  return this->id();
}

bool ServerTraits::hasSigningIdentity() const {
  return this->isEnrollable() || mCustomId.has_value();
}

bool ServerTraits::isEnrollable() const {
  return mEnrollsAs.has_value();
}

bool ServerTraits::hasDataAccess() const {
  return mEnrollsAs == EnrolledParty::RegistrationServer;
}

std::unordered_set<std::string> ServerTraits::userGroups() const {
  std::unordered_set<std::string> result;

  if (this->hasSigningIdentity()) {
    result.emplace(this->defaultId());
    if (mCustomId.has_value()) {
      result.emplace(*mCustomId);
    }
  }

  return result;
}

bool ServerTraits::signingIdentityMatches(const X509CertificateChain& chain) const {
  if (chain.empty()) {
    return false;
  }
  return this->signingIdentityMatches(chain.front());
}

std::optional<std::string> ServerTraits::enrollmentSubject() const {
  if (!this->isEnrollable()) {
    return std::nullopt;
  }
  return this->defaultId();
}

std::string ServerTraits::configNode() const {
  return this->id();
}

std::string ServerTraits::commandLineId() const {
  auto result = this->defaultId();
  boost::algorithm::to_lower(result);
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

}
