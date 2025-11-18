#include <pep/auth/Certificate.hpp>
#include <pep/auth/ServerTraits.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <cassert>

namespace pep {

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

ServerTraits::ServerTraits(std::string abbreviation, std::string description) noexcept
  : mAbbreviation(std::move(abbreviation)), mDescription(std::move(description)) {
}

ServerTraits::ServerTraits(std::string abbreviation, std::string description, EnrolledParty enrollsAs) noexcept
  : ServerTraits(std::move(abbreviation), std::move(description)) {
  assert(enrollsAs != EnrolledParty::User);
  mEnrollsAs = enrollsAs;
}

ServerTraits::ServerTraits(std::string abbreviation, std::string description, std::string customId) noexcept
  : ServerTraits(std::move(abbreviation), std::move(description)) {
  mCustomId = std::move(customId);
}

std::string ServerTraits::configNode() const {
  return this->id();
}

std::string ServerTraits::commandLineId() const {
  auto result = this->defaultId();
  boost::algorithm::to_lower(result);
  return result;
}

std::string ServerTraits::certificateSubject() const {
  return this->id();
}

std::unordered_set<std::string> ServerTraits::certificateSubjects() const {
  return { this->certificateSubject(), this->defaultId() }; // Values may be identical, resulting in an unordered_set<> with just a single entry
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

std::optional<std::string> ServerTraits::userGroup() const {
  if (!this->hasSigningIdentity()) {
    return {};
  }
  return this->certificateSubject();
}

std::unordered_set<std::string> ServerTraits::userGroups() const {
  if (!this->hasSigningIdentity()) {
    return {};
  }
  return this->certificateSubjects();
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

ServerTraits ServerTraits::AccessManager()      { return ServerTraits{ "AM", "Access Manager",      EnrolledParty::AccessManager       }; }
ServerTraits ServerTraits::AuthServer()         { return ServerTraits{ "AS", "Auth Server",         "Authserver"                       }; }
ServerTraits ServerTraits::KeyServer()          { return ServerTraits{ "KS", "Key Server"                                              }; }
ServerTraits ServerTraits::RegistrationServer() { return ServerTraits{ "RS", "Registration Server", EnrolledParty::RegistrationServer, }; }
ServerTraits ServerTraits::StorageFacility()    { return ServerTraits{ "SF", "Storage Facility",    EnrolledParty::StorageFacility     }; }
ServerTraits ServerTraits::Transcryptor()       { return ServerTraits{ "TS", "Transcryptor",        EnrolledParty::Transcryptor,       }; }

std::set<ServerTraits> ServerTraits::All() {
  std::set<ServerTraits> result = {
    AccessManager(),
    AuthServer(),
    KeyServer(),
    RegistrationServer(),
    StorageFacility(),
    Transcryptor(),
  };
  assert(result.size() == 6U);
  return result;
}

std::set<ServerTraits> ServerTraits::Where(const std::function<bool(const ServerTraits&)>& include) {
  auto result = All();
  std::erase_if(result, [include](const ServerTraits& candidate) {return !include(candidate); });
  return result;
}

std::optional<ServerTraits> ServerTraits::Find(const std::function<bool(const ServerTraits&)>& predicate) {
  auto filtered = Where(predicate);
  switch (filtered.size()) {
  case 0U:
    return std::nullopt;
  case 1U:
    return std::move(*filtered.begin());
  default:
    std::vector<std::string> descriptions;
    std::transform(filtered.begin(), filtered.end(), std::back_inserter(descriptions), [](const ServerTraits& traits) {return traits.description(); });
    throw std::runtime_error("Multiple server traits match the predicate: " + boost::join(descriptions, " and "));
  }
}

std::optional<ServerTraits> ServerTraits::Find(EnrolledParty enrollsAs) {
  return Find([enrollsAs](const ServerTraits& candidate) {return candidate.enrollsAs() == enrollsAs; });
}

}
