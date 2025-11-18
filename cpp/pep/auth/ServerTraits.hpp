#pragma once

#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <functional>
#include <set>
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
  // String properties
  const std::string& abbreviation() const noexcept { return mAbbreviation; }
  const std::string& description() const noexcept { return mDescription; }
  std::string configNode() const;
  std::string commandLineId() const;

  // Properties related to certificates (both TLS and signing)
  std::string certificateSubject() const; // The primary one, i.e. the custom ID if available ("Authserver" for AS)
  std::unordered_set<std::string> certificateSubjects() const;

  // Level of access
  bool hasSigningIdentity() const;
  bool isEnrollable() const; // requires/implies hasSigningIdentity()
  bool hasDataAccess() const; // requires/implies isEnrollable()

  // Properties related to signing identities: nullopt/empty/false for servers without a signing identity (KS)
  std::optional<std::string> userGroup() const;
  std::unordered_set<std::string> userGroups() const;
  bool signingIdentityMatches(const std::string& certificateSubject) const;
  bool signingIdentityMatches(const X509Certificate& certificate) const;
  bool signingIdentityMatches(const X509CertificateChain& chain) const;

  // Properties related to enrollment: nullopt for servers that are not enrollable (AS and KS)
  const std::optional<EnrolledParty>& enrollsAs() const noexcept { return mEnrollsAs; }
  std::optional<std::string> enrollmentSubject() const;

  // Allow class to be used as key in an std::set<> or std::map<> (std::unordered_XYZ would require addition of an std::hash<> specialization)
  auto operator<=>(const ServerTraits&) const = default;

  // These could have been static consts
  static ServerTraits AccessManager();      // hasSigningIdentity, isEnrollable
  static ServerTraits AuthServer();         // hasSigningIdentity
  static ServerTraits KeyServer();          // <none>
  static ServerTraits RegistrationServer(); // hasSigningIdentity, isEnrollable, hasDataAccess
  static ServerTraits StorageFacility();    // hasSigningIdentity, isEnrollable
  static ServerTraits Transcryptor();       // hasSigningIdentity, isEnrollable

  // Getting multiple server (traits instances)
  static std::set<ServerTraits> All();
  static std::set<ServerTraits> Where(const std::function<bool(const ServerTraits&)>& include);

  // Getting a single server (traits instance) by property
  static std::optional<ServerTraits> Find(const std::function<bool(const ServerTraits&)>& match);
  static std::optional<ServerTraits> Find(EnrolledParty enrollsAs);
};

}
