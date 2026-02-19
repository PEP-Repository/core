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
  std::optional<EnrolledParty> mEnrollsAsParty;
  std::optional<std::string> mCustomId;

  std::string defaultId() const;
  std::string id() const;

  // Private constructors ensure that all instances have been created by this class itself, i.e. are valid
  ServerTraits(std::string abbreviation, std::string description) noexcept;
  ServerTraits(std::string abbreviation, std::string description, EnrolledParty enrollsAsParty) noexcept;
  ServerTraits(std::string abbreviation, std::string description, std::string customId) noexcept;

public:
  // String properties
  const std::string& abbreviation() const noexcept { return mAbbreviation; }
  const std::string& description() const noexcept { return mDescription; }
  std::string configNode() const;
  std::string commandLineId() const;
  std::string metricsId() const;

  // Properties related to certificates (both TLS and signing)
  std::string certificateSubject() const; // The primary one, i.e. the custom ID if available ("Authserver" for AS)
  std::unordered_set<std::string> certificateSubjects() const;

  // Level of access
  bool hasSigningIdentity() const;
  bool isEnrollable() const; // requires/implies hasSigningIdentity()
  bool hasDataAccess() const; // requires/implies isEnrollable()

  // Properties related to signing identities: nullopt/empty/false for servers without a signing identity (KS)
  std::optional<std::string> userGroup(bool require) const;
  std::unordered_set<std::string> userGroups() const;
  bool signingIdentityMatches(const std::string& certificateSubject) const;
  bool signingIdentityMatches(const X509Certificate& certificate) const;
  bool signingIdentityMatches(const X509CertificateChain& chain) const;

  // Properties related to enrollment: nullopt for servers that are not enrollable (AS and KS)
  const std::optional<EnrolledParty>& enrollsAsParty(bool require) const;
  std::optional<std::string> enrollmentSubject(bool require) const;

  // Allow class to be used as key in (std) containers (also see std::hash<> specialization below)
  auto operator<=>(const ServerTraits&) const = default;

  // Individual servers. Defined as static methods instead of consts to avoid the static initialization fiasco: see e.g. UserGroup::AccessManager and UserGroup::Authserver
  static ServerTraits AccessManager();      // hasSigningIdentity + isEnrollable
  static ServerTraits AuthServer();         // hasSigningIdentity
  static ServerTraits KeyServer();          // <none>
  static ServerTraits RegistrationServer(); // hasSigningIdentity + isEnrollable + hasDataAccess
  static ServerTraits StorageFacility();    // hasSigningIdentity + isEnrollable
  static ServerTraits Transcryptor();       // hasSigningIdentity + isEnrollable

  // Getting/finding multiple servers
  static std::unordered_set<ServerTraits> All();
  static std::unordered_set<ServerTraits> Where(const std::function<bool(const ServerTraits&)>& include);

  // Getting/finding an individual server
  static std::optional<ServerTraits> Find(const std::function<bool(const ServerTraits&)>& match);
  static std::optional<ServerTraits> Find(EnrolledParty enrollsAsParty);
};

}

namespace std {

// Allow ServerTraits to (also) be used as key in unordered (std) containers
template <> struct hash<pep::ServerTraits> {
  size_t operator()(const pep::ServerTraits& server) const;
};

}
