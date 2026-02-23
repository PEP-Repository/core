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

std::string ServerTraits::lowercaseId() const {
  auto result = this->id();
  boost::algorithm::to_lower(result);
  assert(result == boost::algorithm::to_lower_copy(this->defaultId()) && "Default and custom IDs should differ only in cAsInG");
  return result;
}

ServerTraits::ServerTraits(std::string abbreviation, std::string description) noexcept
  : mAbbreviation(std::move(abbreviation)), mDescription(std::move(description)) {
}

ServerTraits::ServerTraits(std::string abbreviation, std::string description, EnrolledParty enrollsAsParty) noexcept
  : ServerTraits(std::move(abbreviation), std::move(description)) {
  assert(enrollsAsParty != EnrolledParty::User);
  mEnrollsAsParty = enrollsAsParty;
}

ServerTraits::ServerTraits(std::string abbreviation, std::string description, std::string customId) noexcept
  : ServerTraits(std::move(abbreviation), std::move(description)) {
  mCustomId = std::move(customId);
}

std::string ServerTraits::configNode() const {
  return this->id();
}

std::string ServerTraits::commandLineId() const {
  return this->lowercaseId();
}

std::string ServerTraits::metricsId() const {
  return this->lowercaseId();
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
  return mEnrollsAsParty.has_value();
}

bool ServerTraits::hasDataAccess() const {
  return mEnrollsAsParty == EnrolledParty::RegistrationServer;
}

std::optional<std::string> ServerTraits::userGroup(bool require) const {
  if (this->hasSigningIdentity()) {
    return this->certificateSubject();
  }
  if (require) {
    throw std::runtime_error(this->description() + " does not have a signing identity");
  }
  return {};
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
  return this->signingIdentityMatches(chain.leaf());
}

const std::optional<EnrolledParty>& ServerTraits::enrollsAsParty(bool require) const {
  if (require && !mEnrollsAsParty.has_value()) {
    throw std::runtime_error(this->description() + " is not enrollable");
  }
  return mEnrollsAsParty;
}

std::optional<std::string> ServerTraits::enrollmentSubject(bool require) const {
  if (this->enrollsAsParty(require)) { // Raises an exception if required but not enrollable
    return this->defaultId();
  }
  return std::nullopt;
}

ServerTraits ServerTraits::AccessManager()      { return ServerTraits{ "AM", "Access Manager",      EnrolledParty::AccessManager       }; }
ServerTraits ServerTraits::AuthServer()         { return ServerTraits{ "AS", "Auth Server",         "Authserver"                       }; }
ServerTraits ServerTraits::KeyServer()          { return ServerTraits{ "KS", "Key Server"                                              }; }
ServerTraits ServerTraits::RegistrationServer() { return ServerTraits{ "RS", "Registration Server", EnrolledParty::RegistrationServer, }; }
ServerTraits ServerTraits::StorageFacility()    { return ServerTraits{ "SF", "Storage Facility",    EnrolledParty::StorageFacility     }; }
ServerTraits ServerTraits::Transcryptor()       { return ServerTraits{ "TS", "Transcryptor",        EnrolledParty::Transcryptor,       }; }

std::unordered_set<ServerTraits> ServerTraits::All() {
  std::unordered_set<ServerTraits> result = {
    AccessManager(),
    AuthServer(),
    KeyServer(),
    RegistrationServer(),
    StorageFacility(),
    Transcryptor(),
  };

  // Ensure our (in)equality comparison doesn't consider instances equivalent
  assert(result.size() == 6U);

  return result;
}

std::unordered_set<ServerTraits> ServerTraits::Where(const std::function<bool(const ServerTraits&)>& include) {
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
    return *filtered.begin();
  default:
    std::vector<std::string> descriptions;
    std::transform(filtered.begin(), filtered.end(), std::back_inserter(descriptions), [](const ServerTraits& traits) {return traits.description(); });
    throw std::runtime_error("Multiple server traits match the predicate: " + boost::join(descriptions, " and "));
  }
}

std::optional<ServerTraits> ServerTraits::Find(EnrolledParty enrollsAsParty) {
  return Find([enrollsAsParty](const ServerTraits& candidate) {return candidate.enrollsAsParty(false) == enrollsAsParty; });
}

}

namespace std {

size_t hash<pep::ServerTraits>::operator()(const pep::ServerTraits& server) const {
  return std::hash<std::string>()(server.abbreviation()); // Unit test checks that abbreviations are unique
}

}
