#pragma once

#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <functional>
#include <unordered_set>

namespace pep {

class ServerTraits {
private:
  std::string mAbbreviation;
  std::string mDescription;
  std::optional<EnrolledParty> mEnrollsAs;
  std::optional<std::string> mCustomId;

  std::string defaultId() const;
  std::string id() const;

  // Private constructors ensure that all instances have been created by this class itself, i.e. are valid
  ServerTraits(std::string abbreviation, std::string description) noexcept;
  ServerTraits(std::string abbreviation, std::string description, EnrolledParty enrollsAs) noexcept;
  ServerTraits(std::string abbreviation, std::string description, std::string customId) noexcept;

public:
  // Properties for all servers
  const std::string& abbreviation() const noexcept { return mAbbreviation; }
  const std::string& description() const noexcept { return mDescription; }
  std::string configNode() const;
  std::string commandLineId() const;
  std::string tlsCertificateSubject() const;

  // Properties for servers with a signing identity (PEP certificate): AM, AS, RS, SF, TS (i.e. all except KS)
  bool hasSigningIdentity() const;
  std::optional<std::string> userGroup() const;
  std::unordered_set<std::string> userGroups() const;
  bool signingIdentityMatches(const std::string& certificateSubject) const;
  bool signingIdentityMatches(const X509Certificate& certificate) const;
  bool signingIdentityMatches(const X509CertificateChain& chain) const;

  // Properties for servers with pseudonym access: AM, RS, SF, TS (i.e. signing servers except AS)
  bool isEnrollable() const;
  const std::optional<EnrolledParty>& enrollsAs() const noexcept { return mEnrollsAs; }
  std::optional<std::string> enrollmentSubject() const;

  // Properties for servers with data access: only RS
  bool hasDataAccess() const;

  // These could have been static consts
  static ServerTraits AccessManager();
  static ServerTraits AuthServer();
  static ServerTraits KeyServer();
  static ServerTraits RegistrationServer();
  static ServerTraits StorageFacility();
  static ServerTraits Transcryptor();

  static std::vector<ServerTraits> All();

  // Finding a single server (traits instance) by property
  static std::optional<ServerTraits> Find(const std::function<bool(const ServerTraits&)>& predicate);
  static std::optional<ServerTraits> Find(EnrolledParty enrollsAs);

  // Allow class to be used as key in an std::map<> (std::unordered_map would require addition of an std::hash<> specialization)
  auto operator<=>(const ServerTraits&) const = default;
};

}
